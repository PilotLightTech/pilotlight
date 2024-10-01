/*
   pl_debug_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
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
#include "pl.h"
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
#include "pl_ext.inc"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plDebugContext
{
    // other
    plDevice*       ptDevice;
    plTempAllocator tTempAllocator;

    // stat data
    double**     sbppdValues;
    const char** ppcNames;
    double***    sbppdFrameValues; // values to write to
    double*      sbdRawValues; // raw values
    bool*        sbbValues;
    uint32_t     uSelectedCount;
    uint32_t     uMaxSelectedCount;
    float        fLegendWidth;

    // profile data
    plProfileSample* sbtSamples;
    float fDeltaTime;
    bool bProfileFirstRun;
} plDebugContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plDebugContext* gptDebugCtx = NULL;

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

    if(!gptDebugCtx->ptDevice)
        gptDebugCtx->ptDevice = gptDataRegistry->get_data("device");
    
    const size_t szHostMemoryInUse = gptGfx->get_host_memory_in_use();
    const size_t szLocalMemoryInUse = gptGfx->get_local_memory_in_use();
    const size_t szMemoryUsage = gptMemory->get_memory_usage();
    const size_t szActiveAllocations = gptMemory->get_allocation_count();
    const size_t szAllocationFrees = gptMemory->get_free_count();

    if(gptUI->begin_window("Memory Allocations", bValue, false))
    {
        gptUI->layout_dynamic(0.0f, 1);
        if(szMemoryUsage > 1000000000)
            gptUI->text("General Memory Usage:       %0.3f gb", (double)szMemoryUsage / 1000000000);
        else if(szMemoryUsage > 1000000)
            gptUI->text("General Memory Usage:       %0.3f mb", (double)szMemoryUsage / 1000000);
        else if(szMemoryUsage > 1000)
            gptUI->text("General Memory Usage:       %0.3f kb", (double)szMemoryUsage / 1000);
        else
            gptUI->text("General Memory Usage:       %llu bytes", (double)szMemoryUsage);
    
        if(szHostMemoryInUse > 1000000000)
            gptUI->text("Host Graphics Memory Usage: %0.3f gb", (double)szHostMemoryInUse / 1000000000);
        else if(szHostMemoryInUse > 1000000)
            gptUI->text("Host Graphics Memory Usage: %0.3f mb", (double)szHostMemoryInUse / 1000000);
        else if(szHostMemoryInUse > 1000)
            gptUI->text("Host Graphics Memory Usage: %0.3f kb", (double)szHostMemoryInUse / 1000);
        else
            gptUI->text("Host Graphics Memory Usage: %llu bytes", (double)szHostMemoryInUse);

        gptUI->text("Active Allocations:         %u", szActiveAllocations);
        gptUI->text("Freed Allocations:          %u", szAllocationFrees);

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

        size_t szOriginalAllocationCount = 0;
        plAllocationEntry* atAllocations = gptMemory->get_allocations(&szOriginalAllocationCount);
        
        plUiClipper tClipper = {(uint32_t)szOriginalAllocationCount};
        while(gptUI->step_clipper(&tClipper))
        {
            for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
            {
                plAllocationEntry tEntry = atAllocations[i];
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

    if(gptUI->begin_window("Profiling (WIP)", bValue, false))
    {
        const plVec2 tWindowSize = gptUI->get_window_size();
        const plVec2 tWindowPos = gptUI->get_window_pos();
        const plVec2 tWindowEnd = pl_add_vec2(tWindowSize, tWindowPos);

        plProfileSample* ptSamples = NULL;
        uint32_t uSampleSize = 0;
        if(gptDebugCtx->bProfileFirstRun && gptIO->ulFrameCount == 1)
        {
            ptSamples = pl_get_last_frame_samples(&uSampleSize);
            pl_sb_resize(gptDebugCtx->sbtSamples, uSampleSize);
            memcpy(gptDebugCtx->sbtSamples, ptSamples, sizeof(plProfileSample) * uSampleSize);
            gptDebugCtx->fDeltaTime = gptIO->fDeltaTime;
            gptDebugCtx->bProfileFirstRun = false;
        }
        else
        {
            ptSamples = gptDebugCtx->sbtSamples;
            uSampleSize = pl_sb_size(gptDebugCtx->sbtSamples);
            gptDebugCtx->bProfileFirstRun = false;
        }

        if(uSampleSize == 0)
        {
            ptSamples = pl_get_last_frame_samples(&uSampleSize);
            gptDebugCtx->fDeltaTime = gptIO->fDeltaTime;
        }

        gptUI->layout_static(0.0f, 150.0f, 2);
        if(pl_sb_size(gptDebugCtx->sbtSamples) == 0)
        {
            if(gptUI->button("Capture Frame"))
            {
                pl_sb_resize(gptDebugCtx->sbtSamples, uSampleSize);
                memcpy(gptDebugCtx->sbtSamples, ptSamples, sizeof(plProfileSample) * uSampleSize);
            }
        }
        else
        {
            if(gptUI->button("Release Frame"))
            {
                pl_sb_reset(gptDebugCtx->sbtSamples);
            }
        }
        gptUI->text("Frame Time: %0.3f", gptDebugCtx->fDeltaTime);

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
                        gptUI->progress_bar((float)(ptSamples[i].dDuration / (double)gptDebugCtx->fDeltaTime), (plVec2){-1.0f, 0.0f}, NULL);
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
                    const double dMaxTime = pl_maxd(gptDebugCtx->fDeltaTime, dVisibleTime);

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
                    const plVec2 tMousePos = gptIOI->get_mouse_pos();
                    const plRect tWidgetRect = pl_calculate_rect(tCursorPos, tTimelineSize);
                    bool bHovered = pl_rect_contains_point(&tWidgetRect, tMousePos);
                    if(bHovered)
                    {
                        
                        const double dStartVisibleTime = dInitialVisibleTime;
                        float fWheel = gptIOI->get_mouse_wheel();
                        if(fWheel < 0)      dInitialVisibleTime += dInitialVisibleTime * 0.2;
                        else if(fWheel > 0) dInitialVisibleTime -= dInitialVisibleTime * 0.2;
                        dInitialVisibleTime = pl_clampd(0.0001, dInitialVisibleTime, gptDebugCtx->fDeltaTime);

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

                        if(gptIOI->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 5.0f))
                        {
                            const plVec2 tWindowScroll = gptUI->get_window_scroll();
                            const plVec2 tMouseDrag = gptIOI->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 5.0f);
                            gptUI->set_window_scroll((plVec2){tWindowScroll.x - tMouseDrag.x, tWindowScroll.y});
                            gptIOI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                        }
                    }
                    
                    gptDraw->add_rect_filled(ptFgLayer, tCursorPos, pl_add_vec2(tCursorPos, tTimelineBarSize), (plVec4){0.5f, 0.0f, 0.0f, 0.7f}, 0.0f, 0);

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
                        char* pcDecimals = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%%0.%uf", uDecimalPlaces);
                        char* pcText0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, pcDecimals, (double)dTime0 * dUnitMultiplier);
  
                        const double dTime1 = dTime0 + dIncrement * 0.5;
                        char* pcText1 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, pcDecimals, (double)dTime1 * dUnitMultiplier);
                        pl_temp_allocator_reset(&gptDebugCtx->tTempAllocator);

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
                        char* pcDecimals = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, " %%0.%uf ms ", uDecimalPlaces);
                        char* pcText0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, pcDecimals, (double)dTime0 * dUnitMultiplier);
                        const plRect tBB0 = gptDraw->calculate_text_bb(gptUI->get_default_font(), gptDraw->get_font(gptUI->get_default_font())->fSize, (plVec2){roundf((float)dLineX0), tCursorPos.y + 20.0f}, pcText0, 0.0f);

                        const double dTime1 = dTime0 + dIncrement;
                        const float dLineX1 = (float)(dTime1 * dConvertToPixel) + tCursorPos.x;
                        char* pcText1 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, pcDecimals, (double)dTime1 * dUnitMultiplier);
                        const plRect tBB1 = gptDraw->calculate_text_bb(gptUI->get_default_font(), gptDraw->get_font(gptUI->get_default_font())->fSize, (plVec2){roundf((float)dLineX1), tCursorPos.y + 20.0f}, pcText1, 0.0f);
                        pl_temp_allocator_reset(&gptDebugCtx->tTempAllocator);

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
                        char* pcDecimals = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%%0.%uf ms", uDecimalPlaces);
                        char* pcText = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, pcDecimals, (double)dCurrentTime * dUnitMultiplier);
                        const float fTextWidth = gptDraw->calculate_text_size(gptUI->get_default_font(), gptDraw->get_font(gptUI->get_default_font())->fSize, pcText, 0.0f).x;
                        gptDraw->add_line(ptFgLayer, (plVec2){fLineX, tCursorPos.y}, (plVec2){fLineX, tCursorPos.y + 20.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        gptDraw->add_text(ptFgLayer, gptUI->get_default_font(), gptDraw->get_font(gptUI->get_default_font())->fSize, (plVec2){roundf(fLineX - fTextWidth / 2.0f), tCursorPos.y + 20.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, pcText, 0.0f);
                        pl_temp_allocator_reset(&gptDebugCtx->tTempAllocator);  
                        dCurrentTime += dIncrement;
                    }

                    for(uint32_t i = 0; i < uSampleSize; i++)
                    {
                        const float fPixelWidth = (float)(dConvertToPixel * ptSamples[i].dDuration);
                        const float fPixelStart = (float)(dConvertToPixel * ptSamples[i].dStartTime);
                        gptUI->layout_space_push(fPixelStart, (float)ptSamples[i].uDepth * 25.0f + 55.0f, fPixelWidth, 20.0f);
                        char* pcTempBuffer = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%s##pro%u", ptSamples[i].pcName, i);
                        gptUI->push_theme_color(PL_UI_COLOR_BUTTON, &atColors[ptSamples[i].uDepth % 6]);
                        if(gptUI->button(pcTempBuffer))
                        {
                            dInitialVisibleTime = pl_clampd(0.0001, ptSamples[i].dDuration, (double)gptDebugCtx->fDeltaTime);
                            const double dNewConvertToPixel = tChildWindowSize.x / dInitialVisibleTime;
                            const double dNewPixelStart = dNewConvertToPixel * (ptSamples[i].dStartTime + 0.5 * ptSamples[i].dDuration);
                            const double dNewScrollX = dNewPixelStart - dNewConvertToPixel * dInitialVisibleTime * 0.5;
                            gptUI->set_window_scroll((plVec2){(float)dNewScrollX, 0.0f});
                        }
                        pl_temp_allocator_reset(&gptDebugCtx->tTempAllocator);
                        if(gptUI->was_last_item_hovered())
                        {
                            // bHovered = false;
                            gptUI->begin_tooltip();
                            gptUI->color_text(atColors[ptSamples[i].uDepth % 6], "%s", ptSamples[i].pcName);
                            gptUI->text("Duration:   %0.7f seconds", ptSamples[i].dDuration);
                            gptUI->text("Start Time: %0.7f seconds", ptSamples[i].dStartTime);
                            gptUI->color_text(atColors[ptSamples[i].uDepth % 6], "Frame Time: %0.2f %%", 100.0 * ptSamples[i].dDuration / (double)gptDebugCtx->fDeltaTime);
                            gptUI->end_tooltip(); 
                        }
                        gptUI->pop_theme_color(1);
                    }

                    if(bHovered)
                    {
                        gptDraw->add_line(ptFgLayer, (plVec2){tMousePos.x, tCursorPos.y}, (plVec2){tMousePos.x, tWindowEnd.y}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        char* pcText = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%0.6f", (double)dConvertToTime * (double)(tMousePos.x - tParentCursorPos.x + gptUI->get_window_scroll().x));
                        gptDraw->add_text(ptFgLayer, gptUI->get_default_font(), gptDraw->get_font(gptUI->get_default_font())->fSize, tMousePos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, pcText, 0.0f);
                        pl_temp_allocator_reset(&gptDebugCtx->tTempAllocator);
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

    const char** apcTempNames = NULL;
    const uint32_t uStatsMaxFrames = gptStats->get_max_frames();

    // selectable values
    
    if(gptDebugCtx->sbbValues == NULL) // first run
    {
        uint32_t uNameCount = 0;
        gptDebugCtx->ppcNames = gptStats->get_names(&uNameCount);
        pl_sb_resize(gptDebugCtx->sbbValues, uNameCount);
        pl_sb_resize(gptDebugCtx->sbppdValues, uNameCount);
        pl_sb_resize(gptDebugCtx->sbppdFrameValues, uNameCount);
        gptDebugCtx->sbbValues[0] = true;
        gptDebugCtx->uSelectedCount++;
        gptDebugCtx->uMaxSelectedCount = gptDebugCtx->uSelectedCount;
        pl_sb_resize(gptDebugCtx->sbdRawValues, uStatsMaxFrames *  gptDebugCtx->uMaxSelectedCount);
        *gptStats->get_counter_data(gptDebugCtx->ppcNames[0]) = gptDebugCtx->sbppdValues[0];
        for(uint32_t i = 0; i < uNameCount; i++)
        {
            gptDebugCtx->sbppdFrameValues[i] = gptStats->get_counter_data(gptDebugCtx->ppcNames[i]);
            float fCurrentWidth = gptDraw->calculate_text_size(gptUI->get_default_font(), gptDraw->get_font(gptUI->get_default_font())->fSize, gptDebugCtx->ppcNames[i], 0.0f).x;
            if(fCurrentWidth > gptDebugCtx->fLegendWidth)
                gptDebugCtx->fLegendWidth = fCurrentWidth;
        }

        gptDebugCtx->fLegendWidth += 5.0f;
    }

    gptUI->set_next_window_size((plVec2){900.0f, 450.0f}, PL_UI_COND_ONCE);
    if(gptUI->begin_window("Statistics", bValue, false))
    {
        static bool bAllowNegative = false;
        gptUI->text("Frame rate: %.0f FPS", gptIO->fFrameRate);
        gptUI->text("Frame time: %.6f s", gptIO->fDeltaTime);
        const plVec2 tCursor = gptUI->get_cursor_pos();

        plDrawLayer2D* ptFgLayer = gptUI->get_window_fg_drawlayer();
        const plVec2 tWindowPos = gptUI->get_window_pos();
        const plVec2 tWindowSize = pl_sub_vec2(gptUI->get_window_size(), pl_sub_vec2(tCursor, tWindowPos));
        
        const plVec2 tWindowEnd = pl_add_vec2(gptUI->get_window_size(), tWindowPos);

        gptUI->layout_template_begin(tWindowSize.y - 15.0f);
        gptUI->layout_template_push_static(gptDebugCtx->fLegendWidth * 1.0f);
        gptUI->layout_template_push_dynamic();
        gptUI->layout_template_end();
      
        if(gptUI->begin_child("left"))
        {
            gptUI->layout_dynamic(0.0f, 1);
 
            const plVec4 tNewHeaderColor = (plVec4){0.0f, 0.5f, 0.0f, 0.75f};
            gptUI->push_theme_color(PL_UI_COLOR_HEADER, &tNewHeaderColor);
            for(uint32_t i = 0; i < pl_sb_size(gptDebugCtx->sbppdValues); i++)
            {
                if(gptUI->selectable(gptDebugCtx->ppcNames[i], &gptDebugCtx->sbbValues[i]))
                {
                    if(gptDebugCtx->sbbValues[i])
                    {
                        gptDebugCtx->uSelectedCount++;
                        if(gptDebugCtx->uSelectedCount > gptDebugCtx->uMaxSelectedCount)
                        {
                            gptDebugCtx->uMaxSelectedCount = gptDebugCtx->uSelectedCount;
                            pl_sb_resize(gptDebugCtx->sbdRawValues, uStatsMaxFrames *  gptDebugCtx->uMaxSelectedCount);
                        }
                        *gptStats->get_counter_data(gptDebugCtx->ppcNames[i]) = gptDebugCtx->sbppdValues[i];
                    }
                    else
                    {
                        gptDebugCtx->uSelectedCount--;
                        *gptStats->get_counter_data(gptDebugCtx->ppcNames[i]) = NULL;
                    }
                }
            }
            gptUI->pop_theme_color(1);
            gptUI->end_child();
        }

        uint32_t uSelectionSlot = 0;
        apcTempNames = pl_temp_allocator_alloc(&gptDebugCtx->tTempAllocator, sizeof(const char*) * gptDebugCtx->uSelectedCount);
        for(uint32_t i = 0; i < pl_sb_size(gptDebugCtx->sbbValues); i++)
        {
            if(gptDebugCtx->sbbValues[i])
            {
                apcTempNames[uSelectionSlot] = gptDebugCtx->ppcNames[i];
                *gptDebugCtx->sbppdFrameValues[i] = &gptDebugCtx->sbdRawValues[uSelectionSlot * uStatsMaxFrames];
                uSelectionSlot++;
            }
            else
            {
                *gptDebugCtx->sbppdFrameValues[i] = NULL;
            }
        }

        
        if(gptUI->begin_tab_bar("stat tabs"))
        {
            if(gptUI->begin_tab("Plot"))
            {
                
                gptUI->layout_template_begin(tWindowSize.y - 30.0f);
                gptUI->layout_template_push_variable(300.0f);
                gptUI->layout_template_push_static(gptDebugCtx->fLegendWidth * 2.0f + 10.0f);
                gptUI->layout_template_end();
                
                const plVec2 tCursor0 = gptUI->get_cursor_pos();
                const plVec2 tPlotSize = {tWindowEnd.x - tCursor0.x - 10.0f - gptDebugCtx->fLegendWidth - 20.0f, tWindowSize.y - 40.0f};
                const plVec2 tLegendSize = {gptDebugCtx->fLegendWidth + 20.0f, tWindowSize.y - 40.0f};
                gptUI->invisible_button("plot", tPlotSize);

                const plVec2 tCursor1 = gptUI->get_cursor_pos();
                gptUI->invisible_button("legend", tLegendSize);
                gptDraw->add_rect_filled(ptFgLayer, 
                    tCursor0, 
                    pl_add_vec2(tCursor0, tPlotSize),
                    (plVec4){0.2f, 0.0f, 0.0f, 0.5f}, 0.0f, 0);
       
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
                    gptDraw->add_text(ptFgLayer, gptUI->get_default_font(), gptDraw->get_font(gptUI->get_default_font())->fSize, (plVec2){roundf(tCursor0.x), roundf((float)dYCenter)}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "0", 0.0f);
                    gptDraw->add_line(ptFgLayer, (plVec2){tCursor0.x, (float)dYCenter}, (plVec2){tCursor0.x + tPlotSize.x, (float)dYCenter}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                }

                bAllowNegative = false;
                for(uint32_t i = 0; i < gptDebugCtx->uSelectedCount; i++)
                {
                    const plVec4* ptColor = &atColors[i % 6];
                    double* dValues = &gptDebugCtx->sbdRawValues[i * uStatsMaxFrames];
                    double dMaxValue = -DBL_MAX;
                    double dMinValue = DBL_MAX;

                    for(uint32_t j = 0; j < uStatsMaxFrames; j++)
                    {
                        if(dValues[j] > dMaxValue) dMaxValue = dValues[j];
                        if(dValues[j] < dMinValue) dMinValue = dValues[j];
                        if(dValues[j] < 0.0) bAllowNegative = true;
                    }

                    double dYRange = dAxisMultiplier * pl_maxd(fabs(dMaxValue), fabs(dMinValue)) * (1.1f + (float)i * 0.1f);

                    const double dConversion = dYRange != 0.0 ? (tPlotSize.y - 15.0f) / dYRange : 0.0;
                    
                    const double dXIncrement = (tPlotSize.x - 50.0f) / uStatsMaxFrames;

                    uint32_t uIndexStart = (uint32_t)gptIO->ulFrameCount;

                    const plVec2 tTextPoint = {tCursor1.x + 5.0f, tCursor1.y + i * 20.0f};
                    gptDraw->add_rect_filled(ptFgLayer, tTextPoint, (plVec2){tTextPoint.x + gptDraw->get_font(gptUI->get_default_font())->fSize, tTextPoint.y + gptDraw->get_font(gptUI->get_default_font())->fSize}, *ptColor, 0.0f, 0);
                    gptDraw->add_text(ptFgLayer, gptUI->get_default_font(), gptDraw->get_font(gptUI->get_default_font())->fSize, (plVec2){roundf(tTextPoint.x + 20.0f), roundf(tTextPoint.y)}, *ptColor, apcTempNames[i], 0.0f);
        
                    for(uint32_t j = 0; j < uStatsMaxFrames - 1; j++)
                    {
                        uint32_t uActualIndex0 = (uIndexStart + j) % uStatsMaxFrames;
                        uint32_t uActualIndex1 = (uIndexStart + j + 1) % uStatsMaxFrames;
                        const plVec2 tLineStart = {tCursor0.x + (float)(j * dXIncrement), (float)(dYCenter - dValues[uActualIndex0] * dConversion)};
                        const plVec2 tLineEnd = {tCursor0.x + (float)((j + 1) * dXIncrement), (float)(dYCenter - dValues[uActualIndex1] * dConversion)};
                        gptDraw->add_line(ptFgLayer, tLineStart, tLineEnd, *ptColor, 1.0f);

                        if(j == uStatsMaxFrames - 2)
                        {
                            char acTextBuffer[32] = {0};
                            pl_sprintf(acTextBuffer, "%0.0f", dValues[uActualIndex1]);
                            gptDraw->add_text(ptFgLayer, gptUI->get_default_font(), gptDraw->get_font(gptUI->get_default_font())->fSize, (plVec2){roundf(tLineEnd.x), roundf(tLineEnd.y) - 6.0f}, *ptColor, acTextBuffer, 0.0f);
                        }
                    }
                    
                }
                gptUI->end_tab();
            }

            if(gptUI->begin_tab("Table"))
            {
                gptUI->layout_template_begin(0.0f);
                gptUI->layout_template_push_static(35.0f);
                for(uint32_t i = 0; i < gptDebugCtx->uSelectedCount; i++)
                    gptUI->layout_template_push_static(150.0f);
                gptUI->layout_template_end();

                gptUI->text("Stat");
                for(uint32_t i = 0; i < gptDebugCtx->uSelectedCount; i++)
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
                for(uint32_t i = 0; i < gptDebugCtx->uSelectedCount; i++)
                    gptUI->layout_template_push_static(150.0f);
                gptUI->layout_template_end();

                uint32_t uIndexStart = (uint32_t)gptIO->ulFrameCount;

                plUiClipper tClipper = {uStatsMaxFrames};
                while(gptUI->step_clipper(&tClipper))
                {
                    for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                    {
                        gptUI->text("%u", i);
                        for(uint32_t j = 0; j < gptDebugCtx->uSelectedCount; j++)
                        {
                            double* dValues = &gptDebugCtx->sbdRawValues[j * uStatsMaxFrames];
                            uint32_t uActualIndex0 = (uIndexStart + j) % uStatsMaxFrames;
                            gptUI->text("%13.6f", dValues[uActualIndex0]);
                        }
                    } 
                }

                gptUI->end_tab();
            }
            gptUI->end_tab_bar();
        }
        pl_temp_allocator_reset(&gptDebugCtx->tTempAllocator);
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

    const size_t szHostMemoryInUse = gptGfx->get_host_memory_in_use();
    const size_t szLocalMemoryInUse = gptGfx->get_local_memory_in_use();

    if(!gptDebugCtx->ptDevice)
        gptDebugCtx->ptDevice = gptDataRegistry->get_data("device");
        
    if(gptUI->begin_window("Device Memory Analyzer", bValue, false))
    {
        plDrawLayer2D* ptFgLayer = gptUI->get_window_fg_drawlayer();
        const plVec2 tWindowSize = gptUI->get_window_size();
        const plVec2 tWindowPos = gptUI->get_window_pos();
        const plVec2 tWindowEnd = pl_add_vec2(tWindowSize, tWindowPos);
        const plVec2 tMousePos = gptIOI->get_mouse_pos();

        if(szLocalMemoryInUse > 1000000000)
            gptUI->text("Device Local Memory: %0.3f gb", (double)szLocalMemoryInUse / 1000000000);
        else if(szLocalMemoryInUse > 1000000)
            gptUI->text("Device Local Memory: %0.3f mb", (double)szLocalMemoryInUse / 1000000);
        else if(szLocalMemoryInUse > 1000)
            gptUI->text("Device Local Memory: %0.3f kb", (double)szLocalMemoryInUse / 1000);
        else
            gptUI->text("Device Local Memory: %llu bytes", (double)szLocalMemoryInUse);

        if(szHostMemoryInUse > 1000000000)
            gptUI->text("Host Memory: %0.3f gb", (double)szHostMemoryInUse / 1000000000);
        else if(szHostMemoryInUse > 1000000)
            gptUI->text("Host Memory: %0.3f mb", (double)szHostMemoryInUse / 1000000);
        else if(szHostMemoryInUse > 1000)
            gptUI->text("Host Memory: %0.3f kb", (double)szHostMemoryInUse / 1000);
        else
            gptUI->text("Host Memory: %llu bytes", (double)szHostMemoryInUse);

        const plDeviceMemoryAllocatorI atAllocators[] = {
            *gptGpuAllocators->get_local_buddy_allocator(gptDebugCtx->ptDevice),
            *gptGpuAllocators->get_local_dedicated_allocator(gptDebugCtx->ptDevice),
            *gptGpuAllocators->get_staging_uncached_allocator(gptDebugCtx->ptDevice),
            *gptGpuAllocators->get_staging_cached_allocator(gptDebugCtx->ptDevice)
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
                gptUI->separator_text(apcAllocatorNames[uAllocatorIndex]);

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

                    char* pcTempBuffer0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "Block %u: %0.1fMB##%u", iCurrentBlock, ((double)ptBlock->ulSize)/1000000.0, uAllocatorIndex);
                    char* pcTempBuffer1 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "Block %u##%u", iCurrentBlock, uAllocatorIndex);

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
                        gptDraw->add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth - 6.0f, 30.0f + tCursor0.y}, tAvailableColor, 1.0f, 0.0f, 0);
                    else
                        gptDraw->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, tAvailableColor, 0.0f, 0);
                    gptUI->invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
                    if(gptUI->was_last_item_hovered())
                    {
                        ulHoveredBlock = (uint64_t)i;
                    }
                    pl_temp_allocator_reset(&gptDebugCtx->tTempAllocator);
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
                    gptDraw->add_rect_filled(ptFgLayer, (plVec2){fStartPos, fYPos}, (plVec2){fStartPos + fAvailableWidth, 30.0f + fYPos}, tWastedColor, 0.0f, 0);
                    gptDraw->add_rect_filled(ptFgLayer, (plVec2){fStartPos, fYPos}, (plVec2){fStartPos + fUsedWidth, 30.0f + fYPos}, tUsedColor, 0.0f, 0);

                    if(ptRange->ulBlockIndex == ulHoveredBlock)
                    {
                        const plRect tHitBox = pl_calculate_rect((plVec2){fStartPos, fYPos}, (plVec2){fAvailableWidth, 30});
                        if(pl_rect_contains_point(&tHitBox, tMousePos))
                        {
                            gptDraw->add_rect(ptFgLayer, tHitBox.tMin, tHitBox.tMax, tWhiteColor, 1.0f, 0.0f, 0);
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
            uint32_t uChannelCount = (uint32_t)pl_get_log_channel_count();
            for(uint32_t i = 0; i < uChannelCount; i++)
            {

                plLogChannelInfo tInfo = {0};
                bool bResult = pl_get_log_channel_info(i, &tInfo);

                uint64_t uEntryCount = tInfo.uEntryCount;
                plLogEntry* ptEntries = tInfo.ptEntries;

                if(tInfo.tType & PL_CHANNEL_TYPE_CYCLIC_BUFFER)
                {
                    if(gptUI->begin_tab(tInfo.pcName))
                    {
                        const plVec2 tCursorPos = gptUI->get_cursor_pos();
                        gptUI->layout_dynamic(tWindowEnd.y - tCursorPos.y - 20.0f, 1);
                        if(gptUI->begin_child(tInfo.pcName))
                        {
                            gptUI->layout_dynamic(0.0f, 1);
                            const uint32_t uIndexStart = (uint32_t)uEntryCount;
                            const uint32_t uLogCount = (uint32_t)pl_min(tInfo.uEntryCapacity, uEntryCount);

                            
                            if(bUseClipper)
                            {
                                plUiClipper tClipper = {(uint32_t)uLogCount};
                                while(gptUI->step_clipper(&tClipper))
                                {
                                    for(uint32_t j = tClipper.uDisplayStart; j < tClipper.uDisplayEnd; j++)
                                    {
                                        uint32_t uActualIndex0 = (uIndexStart + j) % (uint32_t)uLogCount;
                                        const plLogEntry* ptEntry = &ptEntries[uActualIndex0];
                                        gptUI->color_text(atColors[ptEntry->uLevel / 1000 - 5], &tInfo.pcBuffer[ptEntry->uOffset]);
                                    } 
                                }
                            }
                            else
                            {
                                    for(uint32_t j = i; j < uLogCount; j++)
                                    {
                                        uint32_t uActualIndex0 = (uIndexStart + j) % (uint32_t)uLogCount;
                                        const plLogEntry* ptEntry = &ptEntries[uActualIndex0];
                                        if(bActiveLevels[ptEntry->uLevel / 1000 - 5])
                                        {
                                            gptUI->color_text(atColors[ptEntry->uLevel / 1000 - 5], &tInfo.pcBuffer[ptEntry->uOffset]);
                                        }
                                    }  
                            }
                            gptUI->end_child();
                        }
                        gptUI->end_tab();
                    }
                }
                else if(tInfo.tType & PL_CHANNEL_TYPE_BUFFER)
                {
                    if(gptUI->begin_tab(tInfo.pcName))
                    {
                        const plVec2 tCursorPos = gptUI->get_cursor_pos();
                        gptUI->layout_dynamic(tWindowEnd.y - tCursorPos.y - 20.0f, 1);
                        if(gptUI->begin_child(tInfo.pcName))
                        {
                            gptUI->layout_dynamic(0.0f, 1);

                            if(bUseClipper)
                            {
                                plUiClipper tClipper = {(uint32_t)uEntryCount};
                                while(gptUI->step_clipper(&tClipper))
                                {
                                    for(uint32_t j = tClipper.uDisplayStart; j < tClipper.uDisplayEnd; j++)
                                    {
                                        plLogEntry* ptEntry = &ptEntries[j];
                                        gptUI->color_text(atColors[ptEntry->uLevel / 1000 - 5], &tInfo.pcBuffer[ptEntry->uOffset]);
                                    } 
                                }
                            }
                            else
                            {
                                for(uint32_t j = 0; j < uEntryCount; j++)
                                {
                                    plLogEntry* ptEntry = &ptEntries[j];
                                    if(bActiveLevels[ptEntry->uLevel / 1000 - 5])
                                        gptUI->color_text(atColors[ptEntry->uLevel / 1000 - 5], &tInfo.pcBuffer[ptEntry->uOffset]);
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

static void
pl_load_debug_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    ptApiRegistry->add(PL_API_DEBUG, pl_load_debug_api());
    if(bReload)
    {
        gptDebugCtx = gptDataRegistry->get_data("plDebugContext");
    }
    else // first load
    {
        static plDebugContext gtDebugCtx = {
            .bProfileFirstRun = true
        };
        gptDebugCtx = &gtDebugCtx;
        gptDataRegistry->set_data("plDebugContext", gptDebugCtx);
    }
}

static void
pl_unload_debug_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    ptApiRegistry->remove(pl_load_debug_api());

    if(bReload)
        return;
        
    pl_sb_free(gptDebugCtx->sbppdValues);
    pl_sb_free(gptDebugCtx->sbppdFrameValues);
    pl_sb_free(gptDebugCtx->sbdRawValues);
    pl_sb_free(gptDebugCtx->sbbValues);
    pl_sb_free(gptDebugCtx->sbtSamples);
    pl_temp_allocator_free(&gptDebugCtx->tTempAllocator);
    gptDebugCtx = NULL;
}
