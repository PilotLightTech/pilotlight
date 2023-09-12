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
#include "pl_ui.h"

// extra
#include "pl_profile.h"
#include "pl_memory.h"
#include "pl_log.h"

// extensions
#include "pl_ui.h"
#include "pl_ui_internal.h"
#include "pl_stats_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

// apis
static const plApiRegistryApiI*   gptApiRegistry  = NULL;
static const plStatsApiI*         ptStatsApi      = NULL;
static const plDataRegistryApiI*  ptDataRegistry  = NULL;

// contexts
static plMemoryContext* ptMemoryCtx = NULL;
static plIO*            ptIOCtx     = NULL;

// other
// static plDevice*       ptDevice       = NULL;
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

const plDebugApiI*
pl_load_debug_api(void)
{
    static const plDebugApiI tApi = {
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

    // if(ptInfo->bShowDeviceMemoryAnalyzer)
    // {
    //     pl_begin_profile_sample("Device Memory Analyzer");
    //     pl__show_device_memory(&ptInfo->bShowDeviceMemoryAnalyzer);
    //     pl_end_profile_sample();
    // }

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

    if(pl_begin_window("Memory Allocations", bValue, false))
    {
        pl_layout_dynamic(0.0f, 1);

        pl_text("Active Allocations: %u", ptMemoryCtx->szActiveAllocations);
        pl_text("Freed Allocations: %u", ptMemoryCtx->szAllocationFrees);

        static char pcFile[1024] = {0};

        pl_layout_template_begin(30.0f);
        pl_layout_template_push_static(50.0f);
        pl_layout_template_push_variable(300.0f);
        pl_layout_template_push_variable(50.0f);
        pl_layout_template_push_variable(50.0f);
        pl_layout_template_end();

        pl_text("%s", "Entry");
        pl_text("%s", "File");
        pl_text("%s", "Line");
        pl_text("%s", "Size");

        pl_layout_dynamic(0.0f, 1);
        pl_separator();

        pl_layout_template_begin(30.0f);
        pl_layout_template_push_static(50.0f);
        pl_layout_template_push_variable(300.0f);
        pl_layout_template_push_variable(50.0f);
        pl_layout_template_push_variable(50.0f);
        pl_layout_template_end();

        const uint32_t uOriginalAllocationCount = pl_sb_size(ptMemoryCtx->sbtAllocations);
        
        plUiClipper tClipper = {uOriginalAllocationCount};
        while(pl_step_clipper(&tClipper))
        {
            for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
            {
                plAllocationEntry tEntry = ptMemoryCtx->sbtAllocations[i];
                strncpy(pcFile, tEntry.pcFile, 1024);
                pl_text("%i", i);
                pl_text("%s", pcFile);
                pl_text("%i", tEntry.iLine);
                pl_text("%u", tEntry.szSize);
            } 
        }
        pl_end_window();
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

    if(pl_begin_window("Profiling", bValue, false))
    {
        const plVec2 tWindowSize = pl_get_window_size();
        const plVec2 tWindowPos = pl_get_window_pos();
        const plVec2 tWindowEnd = pl_add_vec2(tWindowSize, tWindowPos);

        plProfileSample* ptSamples = sbtSamples;
        uint32_t uSampleSize = pl_sb_size(sbtSamples);
        if(uSampleSize == 0)
        {
            ptSamples = pl_get_last_frame_samples(&uSampleSize);
            fDeltaTime = ptIOCtx->fDeltaTime;
        }

        pl_layout_static(0.0f, 100.0f, 1);
        if(pl_sb_size(sbtSamples) == 0)
        {
            if(pl_button("Capture Frame"))
            {
                pl_sb_resize(sbtSamples, uSampleSize);
                memcpy(sbtSamples, ptSamples, sizeof(plProfileSample) * uSampleSize);
            }
        }
        else
        {
            if(pl_button("Release Frame"))
            {
                pl_sb_reset(sbtSamples);
            }
        }

        pl_layout_dynamic(0.0f, 1);

        pl_separator();

        if(pl_begin_tab_bar("profiling tabs"))
        {
            if(pl_begin_tab("Table"))
            {
                pl_layout_template_begin(0.0f);
                pl_layout_template_push_variable(300.0f);
                pl_layout_template_push_variable(50.0f);
                pl_layout_template_push_variable(50.0f);
                pl_layout_template_push_variable(100.0f);
                pl_layout_template_end();

                pl_text("Sample Name");
                pl_text("Time (ms)");
                pl_text("Start (ms)");
                pl_text("Frame Time");

                pl_layout_dynamic(0.0f, 1);
                pl_separator();

                pl_layout_template_begin(0.0f);
                pl_layout_template_push_variable(300.0f);
                pl_layout_template_push_variable(50.0f);
                pl_layout_template_push_variable(50.0f);
                pl_layout_template_push_variable(100.0f);
                pl_layout_template_end();

                plUiClipper tClipper = {uSampleSize};

                const plVec4 tOriginalProgressColor = pl_get_ui_context()->tColorScheme.tProgressBarCol;
                plVec4* tTempProgressColor = &pl_get_ui_context()->tColorScheme.tProgressBarCol;
                while(pl_step_clipper(&tClipper))
                {
                    for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                    {
                        pl_indent(15.0f * (float)(ptSamples[i].uDepth + 1));
                        pl_color_text(atColors[ptSamples[i].uDepth % 6], "%s", ptSamples[i].pcName);
                        pl_unindent(15.0f * (float)(ptSamples[i].uDepth + 1));
                        pl_text("%7.3f", ptSamples[i].dDuration * 1000.0);
                        pl_text("%7.3f", ptSamples[i].dStartTime * 1000.0);
                        *tTempProgressColor = atColors[ptSamples[i].uDepth % 6];
                        pl_progress_bar((float)(ptSamples[i].dDuration / (double)fDeltaTime), (plVec2){-1.0f, 0.0f}, NULL);
                    } 
                }
                *tTempProgressColor = tOriginalProgressColor;

                pl_end_tab();
            }

            if(pl_begin_tab("Graph"))
            {

                const plVec2 tParentCursorPos = pl_get_cursor_pos();
                pl_layout_dynamic(tWindowEnd.y - tParentCursorPos.y - 5.0f, 1);
                if(pl_begin_child("timeline"))
                {

                    const plVec2 tChildWindowSize = pl_get_window_size();
                    const plVec2 tCursorPos = pl_get_cursor_pos();
                    pl_layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, pl_get_window_size().y - 50.0f, uSampleSize + 1);

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
                    plDrawLayer* ptFgLayer = pl_get_window_fg_drawlayer();
                    
                    pl_layout_space_push(0.0f, 0.0f, (float)(dMaxTime * dConvertToPixel), 50.0f);
                    const plVec2 tTimelineSize = {(float)(dMaxTime * dConvertToPixel), tWindowEnd.y - tParentCursorPos.y - 15.0f};
                    const plVec2 tTimelineBarSize = {(float)(dMaxTime * dConvertToPixel), 50.0f};
                    pl_invisible_button("hitregion", tTimelineSize);
                    bool bHovered = pl_was_last_item_hovered();
                    if(bHovered)
                    {
                        
                        const double dStartVisibleTime = dInitialVisibleTime;
                        float fWheel = pl_get_mouse_wheel();
                        if(fWheel < 0)      dInitialVisibleTime += dInitialVisibleTime * 0.2;
                        else if(fWheel > 0) dInitialVisibleTime -= dInitialVisibleTime * 0.2;
                        dInitialVisibleTime = pl_clampd(0.0001, dInitialVisibleTime, fDeltaTime);

                        if(fWheel != 0)
                        {
                            const double dNewConvertToPixel = tChildWindowSize.x / dInitialVisibleTime;
                            const double dNewConvertToTime = dInitialVisibleTime / tChildWindowSize.x;

                            const plVec2 tMousePos = pl_get_mouse_pos();
                            const double dTimeHovered = (double)dConvertToTime * (double)(tMousePos.x - tParentCursorPos.x + pl_get_window_scroll().x);
                            const float fConservedRatio = (tMousePos.x - tParentCursorPos.x) / tChildWindowSize.x;
                            const double dOldPixelStart = dConvertToPixel * dTimeHovered;
                            const double dNewPixelStart = dNewConvertToPixel * (dTimeHovered - fConservedRatio * dInitialVisibleTime);
                            pl_set_window_scroll((plVec2){(float)dNewPixelStart, 0.0f});
                        }

                        if(pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 5.0f))
                        {
                            const plVec2 tWindowScroll = pl_get_window_scroll();
                            const plVec2 tMouseDrag = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 5.0f);
                            pl_set_window_scroll((plVec2){tWindowScroll.x - tMouseDrag.x, tWindowScroll.y});
                            pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                        }
                    }
                    
                    pl_add_rect_filled(ptFgLayer, tCursorPos, pl_add_vec2(tCursorPos, tTimelineBarSize), (plVec4){0.5f, 0.0f, 0.0f, 0.7f});

                    const double dUnitMultiplier = 1000.0;
                    uint32_t uDecimalPlaces = 0;

                    // major ticks
                    const float fScrollStartPosPixel = pl_get_window_scroll().x;
                    const float fScrollEndPosPixel = fScrollStartPosPixel + pl_get_window_size().x;
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
                        const plRect tBB0 = pl_calculate_text_bb(pl_get_default_font(), 13.0f, (plVec2){roundf((float)dLineX0), tCursorPos.y + 20.0f}, pcText0, 0.0f);

                        const double dTime1 = dTime0 + dIncrement;
                        const float dLineX1 = (float)(dTime1 * dConvertToPixel) + tCursorPos.x;
                        char* pcText1 = pl_temp_allocator_sprintf(&tTempAllocator, pcDecimals, (double)dTime1 * dUnitMultiplier);
                        const plRect tBB1 = pl_calculate_text_bb(pl_get_default_font(), 13.0f, (plVec2){roundf((float)dLineX1), tCursorPos.y + 20.0f}, pcText1, 0.0f);
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
                        pl_add_line(ptFgLayer, (plVec2){fLineX, tCursorPos.y}, (plVec2){fLineX, tCursorPos.y + 10.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        dCurrentTime += dIncrement * 0.5f;
                    }

                    // micro ticks
                    dCurrentTime = dStartTime - dIncrement * 0.1f;
                    while(dCurrentTime < dEndTime)
                    {
                        const float fLineX = (float)((dCurrentTime * dConvertToPixel)) + tCursorPos.x;
                        pl_add_line(ptFgLayer, (plVec2){fLineX, tCursorPos.y}, (plVec2){fLineX, tCursorPos.y + 5.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        dCurrentTime += dIncrement * 0.1f;
                    }

                    // major ticks
                    dCurrentTime = dStartTime;
                    while(dCurrentTime < dEndTime)
                    {
                        const float fLineX = (float)((dCurrentTime * dConvertToPixel)) + tCursorPos.x;
                        char* pcDecimals = pl_temp_allocator_sprintf(&tTempAllocator, "%%0.%uf ms", uDecimalPlaces);
                        char* pcText = pl_temp_allocator_sprintf(&tTempAllocator, pcDecimals, (double)dCurrentTime * dUnitMultiplier);
                        const float fTextWidth = pl_calculate_text_size(pl_get_default_font(), 13.0f, pcText, 0.0f).x;
                        pl_add_line(ptFgLayer, (plVec2){fLineX, tCursorPos.y}, (plVec2){fLineX, tCursorPos.y + 20.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        pl_add_text(ptFgLayer, pl_get_default_font(), 13.0f, (plVec2){roundf(fLineX - fTextWidth / 2.0f), tCursorPos.y + 20.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, pcText, 0.0f);
                        pl_temp_allocator_reset(&tTempAllocator);  
                        dCurrentTime += dIncrement;
                    }

                    const plVec4 tOriginalButtonColor = pl_get_ui_context()->tColorScheme.tButtonCol;
                    plVec4* tTempButtonColor = &pl_get_ui_context()->tColorScheme.tButtonCol;
                    for(uint32_t i = 0; i < uSampleSize; i++)
                    {
                        const float fPixelWidth = (float)(dConvertToPixel * ptSamples[i].dDuration);
                        const float fPixelStart = (float)(dConvertToPixel * ptSamples[i].dStartTime);
                        pl_layout_space_push(fPixelStart, (float)ptSamples[i].uDepth * 25.0f + 55.0f, fPixelWidth, 20.0f);
                        char* pcTempBuffer = pl_temp_allocator_sprintf(&tTempAllocator, "%s##pro%u", ptSamples[i].pcName, i);
                        *tTempButtonColor = atColors[ptSamples[i].uDepth % 6];
                        if(pl_button(pcTempBuffer))
                        {
                            dInitialVisibleTime = pl_clampd(0.0001, ptSamples[i].dDuration, (double)fDeltaTime);
                            const double dNewConvertToPixel = tChildWindowSize.x / dInitialVisibleTime;
                            const double dNewPixelStart = dNewConvertToPixel * (ptSamples[i].dStartTime + 0.5 * ptSamples[i].dDuration);
                            const double dNewScrollX = dNewPixelStart - dNewConvertToPixel * dInitialVisibleTime * 0.5;
                            pl_set_window_scroll((plVec2){(float)dNewScrollX, 0.0f});
                        }
                        pl_temp_allocator_reset(&tTempAllocator);
                        if(pl_was_last_item_hovered())
                        {
                            bHovered = false;
                            pl_begin_tooltip();
                            pl_color_text(atColors[ptSamples[i].uDepth % 6], "%s", ptSamples[i].pcName);
                            pl_text("Duration:   %0.7f seconds", ptSamples[i].dDuration);
                            pl_text("Start Time: %0.7f seconds", ptSamples[i].dStartTime);
                            pl_color_text(atColors[ptSamples[i].uDepth % 6], "Frame Time: %0.2f %%", 100.0 * ptSamples[i].dDuration / (double)fDeltaTime);
                            pl_end_tooltip(); 
                        }
                    }
                    *tTempButtonColor = tOriginalButtonColor;

                    if(bHovered)
                    {
                        const plVec2 tMousePos = pl_get_mouse_pos();
                        pl_add_line(ptFgLayer, (plVec2){tMousePos.x, tCursorPos.y}, (plVec2){tMousePos.x, tWindowEnd.y}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        char* pcText = pl_temp_allocator_sprintf(&tTempAllocator, "%0.6f", (double)dConvertToTime * (double)(tMousePos.x - tParentCursorPos.x + pl_get_window_scroll().x));
                        pl_add_text(ptFgLayer, pl_get_default_font(), 13.0f, tMousePos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, pcText, 0.0f);
                        pl_temp_allocator_reset(&tTempAllocator);
                    }

                    pl_layout_space_end();

                    pl_end_child();
                }
                pl_end_tab();
            }
            pl_end_tab_bar();
        }
        pl_end_window();
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
            float fCurrentWidth = pl_calculate_text_size(pl_get_default_font(), 13.0f, ppcNames[i], 0.0f).x;
            if(fCurrentWidth > fLegendWidth)
                fLegendWidth = fCurrentWidth;
        }
    }

    if(pl_begin_window("Statistics", bValue, false))
    {
        pl_text("Frame rate: %.0f FPS", ptIOCtx->fFrameRate);
        pl_text("Frame time: %.6f ms", ptIOCtx->fDeltaTime);
        const plVec2 tCursor = pl_get_cursor_pos();

        plDrawLayer* ptFgLayer = pl_get_window_fg_drawlayer();
        const plVec2 tWindowPos = pl_get_window_pos();
        const plVec2 tWindowSize = pl_sub_vec2(pl_get_window_size(), pl_sub_vec2(tCursor, tWindowPos));
        
        const plVec2 tWindowEnd = pl_add_vec2(pl_get_window_size(), tWindowPos);

        pl_layout_template_begin(tWindowSize.y - 15.0f);
        pl_layout_template_push_static(fLegendWidth * 2.0f);
        pl_layout_template_push_dynamic();
        pl_layout_template_end();
      
        if(pl_begin_child("left"))
        {
            pl_layout_dynamic(0.0f, 1);
 
            const plVec4 tOriginalButtonColor = pl_get_ui_context()->tColorScheme.tHeaderCol;
            pl_get_ui_context()->tColorScheme.tHeaderCol = (plVec4){0.0f, 0.5f, 0.0f, 0.75f};
            for(uint32_t i = 0; i < pl_sb_size(sbppdValues); i++)
            {
                if(pl_selectable(ppcNames[i], &sbbValues[i]))
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
            pl_get_ui_context()->tColorScheme.tHeaderCol = tOriginalButtonColor;
            pl_end_child();
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

        
        if(pl_begin_tab_bar("stat tabs"))
        {
            if(pl_begin_tab("Plot"))
            {
                
                pl_layout_template_begin(tWindowSize.y - 30.0f);
                pl_layout_template_push_variable(300.0f);
                pl_layout_template_push_static(fLegendWidth * 2.0f + 10.0f);
                pl_layout_template_end();
                
                const plVec2 tCursor0 = pl_get_cursor_pos();
                const plVec2 tPlotSize = {tWindowEnd.x - tCursor0.x - 10.0f - fLegendWidth - 20.0f, tWindowSize.y - 40.0f};
                const plVec2 tLegendSize = {fLegendWidth + 20.0f, tWindowSize.y - 40.0f};
                pl_invisible_button("plot", tPlotSize);

                const plVec2 tCursor1 = pl_get_cursor_pos();
                pl_invisible_button("legend", tLegendSize);
                pl_add_rect_filled(ptFgLayer, 
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
                pl_add_text(ptFgLayer, pl_get_default_font(), 13.0f, (plVec2){roundf(tCursor0.x), roundf((float)dYCenter)}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "0", 0.0f);
                pl_add_line(ptFgLayer, (plVec2){tCursor0.x, (float)dYCenter}, (plVec2){tCursor0.x + tPlotSize.x, (float)dYCenter}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);

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
                    pl_add_rect_filled(ptFgLayer, tTextPoint, (plVec2){tTextPoint.x + 13.0f, tTextPoint.y + 13.0f}, *ptColor);
                    pl_add_text(ptFgLayer, pl_get_default_font(), 13.0f, (plVec2){roundf(tTextPoint.x + 15.0f), roundf(tTextPoint.y)}, *ptColor, apcTempNames[i], 0.0f);
        
                    for(uint32_t j = 0; j < PL_STATS_MAX_FRAMES - 1; j++)
                    {
                        uint32_t uActualIndex0 = (uIndexStart + j) % PL_STATS_MAX_FRAMES;
                        uint32_t uActualIndex1 = (uIndexStart + j + 1) % PL_STATS_MAX_FRAMES;
                        const plVec2 tLineStart = {tCursor0.x + (float)(j * dXIncrement), (float)(dYCenter - dValues[uActualIndex0] * dConversion)};
                        const plVec2 tLineEnd = {tCursor0.x + (float)((j + 1) * dXIncrement), (float)(dYCenter - dValues[uActualIndex1] * dConversion)};
                        pl_add_line(ptFgLayer, tLineStart, tLineEnd, *ptColor, 1.0f);
                    }
                    
                }
                pl_end_tab();
            }

            if(pl_begin_tab("Table"))
            {
                pl_layout_template_begin(0.0f);
                pl_layout_template_push_static(35.0f);
                for(uint32_t i = 0; i < uSelectedCount; i++)
                    pl_layout_template_push_static(100.0f);
                pl_layout_template_end();

                pl_text("Stat");
                for(uint32_t i = 0; i < uSelectedCount; i++)
                {
                    const float fXPos = pl_get_cursor_pos().x - 5.0f;
                    const float fYPos = pl_get_cursor_pos().y;
                    pl_add_line(ptFgLayer, (plVec2){fXPos, fYPos}, (plVec2){fXPos, 3000.0f}, (plVec4){0.7f, 0.0f, 0.0f, 1.0f}, 1.0f);
                    pl_button(apcTempNames[i]);
                }

                pl_layout_dynamic(0.0f, 1);
                pl_separator();

                pl_layout_template_begin(0.0f);
                pl_layout_template_push_static(35.0f);
                for(uint32_t i = 0; i < uSelectedCount; i++)
                    pl_layout_template_push_static(100.0f);
                pl_layout_template_end();

                uint32_t uIndexStart = (uint32_t)ptIOCtx->ulFrameCount;

                plUiClipper tClipper = {PL_STATS_MAX_FRAMES};
                while(pl_step_clipper(&tClipper))
                {
                    for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                    {
                        pl_text("%u", i);
                        for(uint32_t j = 0; j < uSelectedCount; j++)
                        {
                            double* dValues = &sbdRawValues[j * PL_STATS_MAX_FRAMES];
                            uint32_t uActualIndex0 = (uIndexStart + j) % PL_STATS_MAX_FRAMES;
                            pl_text("%13.6f", dValues[uActualIndex0]);
                        }
                    } 
                }

                pl_end_tab();
            }
            pl_end_tab_bar();
        }
        pl_temp_allocator_reset(&tTempAllocator);
        pl_end_window();
    } 
}

// static void
// pl__show_device_memory(bool* bValue)
// {
//     if(!ptDevice)
//         ptDevice = ptDataRegistry->get_data("device");
        
//     if(pl_begin_window("Device Memory Analyzer", bValue, false))
//     {
//         plDrawLayer* ptFgLayer = pl_get_window_fg_drawlayer();
//         const plVec2 tWindowSize = pl_get_window_size();
//         const plVec2 tWindowPos = pl_get_window_pos();
//         const plVec2 tWindowEnd = pl_add_vec2(tWindowSize, tWindowPos);

//         uint32_t uBlockCount = 0;
//         plDeviceAllocationBlock* sbtBlocks = ptDevice->tStagingCachedAllocator.blocks(ptDevice->tStagingCachedAllocator.ptInst, &uBlockCount);

//         if(uBlockCount > 0)
//         {
//             pl_text("Device Memory: Staging Cached");

//             pl_layout_template_begin(30.0f);
//             pl_layout_template_push_static(150.0f);
//             pl_layout_template_push_variable(300.0f);
//             pl_layout_template_end();

//             static const uint64_t ulMaxBlockSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE;

//             for(uint32_t i = 0; i < uBlockCount; i++)
//             {
//                 // ptAppData->tTempAllocator
//                 plDeviceAllocationBlock* ptBlock = &sbtBlocks[i];
//                 char* pcTempBuffer0 = pl_temp_allocator_sprintf(&tTempAllocator, "Block %u: %0.1fMB##sc", i, ((double)ptBlock->ulSize)/1000000.0);
//                 char* pcTempBuffer1 = pl_temp_allocator_sprintf(&tTempAllocator, "Block %u##sc", i);

//                 pl_button(pcTempBuffer0);
                

//                 plVec2 tCursor0 = pl_get_cursor_pos();
//                 const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
//                 float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)ulMaxBlockSize;
//                 float fUsedWidth = fWidthAvailable * ((float)ptBlock->sbtRanges[0].tAllocation.ulSize) / (float)ulMaxBlockSize;

//                 pl_invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
//                 pl_add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f});
//                 pl_add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f});
//                 if(pl_was_last_item_active())
//                     pl_add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  fTotalWidth, 30.0f + tCursor0.y}, (plVec4){ 1.f, 1.0f, 1.0f, 1.0f}, 2.0f);

//                 if(pl_was_last_item_hovered())
//                 {
//                     pl_begin_tooltip();
//                     pl_text(ptBlock->sbtRanges[0].pcName);
//                     pl_end_tooltip();
//                 }

//                 pl_temp_allocator_reset(&tTempAllocator);
//             }

//         }

//         sbtBlocks = ptDevice->tStagingUnCachedAllocator.blocks(ptDevice->tStagingUnCachedAllocator.ptInst, &uBlockCount);
//         if(uBlockCount > 0)
//         {
//             pl_layout_dynamic(0.0f, 1);
//             pl_separator();
//             pl_text("Device Memory: Staging Uncached");

//             pl_layout_template_begin(30.0f);
//             pl_layout_template_push_static(150.0f);
//             pl_layout_template_push_variable(300.0f);
//             pl_layout_template_end();

//             static const uint64_t ulMaxBlockSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE;

//             for(uint32_t i = 0; i < uBlockCount; i++)
//             {
//                 plDeviceAllocationBlock* ptBlock = &sbtBlocks[i];
//                 char* pcTempBuffer0 = pl_temp_allocator_sprintf(&tTempAllocator, "Block %u: %0.1fMB##suc", i, ((double)ptBlock->ulSize)/1000000.0);
//                 char* pcTempBuffer1 = pl_temp_allocator_sprintf(&tTempAllocator, "Block %u##suc", i);

//                 pl_button(pcTempBuffer0);
                

//                 plVec2 tCursor0 = pl_get_cursor_pos();
//                 const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
//                 float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)ulMaxBlockSize;
//                 float fUsedWidth = fWidthAvailable * ((float)ptBlock->sbtRanges[0].tAllocation.ulSize) / (float)ulMaxBlockSize;

//                 pl_invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
//                 pl_add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f});
//                 pl_add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f});
//                 if(pl_was_last_item_active())
//                     pl_add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  fTotalWidth, 30.0f + tCursor0.y}, (plVec4){ 1.f, 1.0f, 1.0f, 1.0f}, 2.0f);

//                 if(pl_was_last_item_hovered())
//                 {
//                     pl_begin_tooltip();
//                     pl_text(ptBlock->sbtRanges[0].pcName);
//                     pl_end_tooltip();
//                 }

//                 pl_temp_allocator_reset(&tTempAllocator);
//             }

//         }

//         sbtBlocks = ptDevice->tLocalBuddyAllocator.blocks(ptDevice->tLocalBuddyAllocator.ptInst, &uBlockCount);
//         if(uBlockCount > 0)
//         {

//             pl_layout_dynamic(0.0f, 1);
//             pl_separator();
//             pl_text("Device Memory: Local Buddy");

//             pl_layout_template_begin(30.0f);
//             pl_layout_template_push_static(150.0f);
//             pl_layout_template_push_variable(300.0f);
//             pl_layout_template_end();

//             uint32_t uNodeCount = 0;
//             plDeviceAllocationNode* sbtNodes = ptDevice->tLocalBuddyAllocator.nodes(ptDevice->tLocalBuddyAllocator.ptInst, &uNodeCount);
//             char** sbDebugNames = ptDevice->tLocalBuddyAllocator.names(ptDevice->tLocalBuddyAllocator.ptInst, &uNodeCount);

//             const uint32_t uNodesPerBlock = uNodeCount / uBlockCount;
//             for(uint32_t i = 0; i < uBlockCount; i++)
//             {
//                 plDeviceAllocationBlock* ptBlock = &sbtBlocks[i];
//                 char* pcTempBuffer0 = pl_temp_allocator_sprintf(&tTempAllocator, "Block %u: 256 MB##b", i);
//                 char* pcTempBuffer1 = pl_temp_allocator_sprintf(&tTempAllocator, "Block %u ##b", i);
//                 pl_button(pcTempBuffer0);

//                 plVec2 tCursor0 = pl_get_cursor_pos();
//                 const plVec2 tMousePos = pl_get_mouse_pos();
//                 uint32_t uHoveredNode = 0;
//                 const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
//                 float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
//                 pl_invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
//                 pl_add_rect_filled(ptFgLayer, (plVec2){tCursor0.x, tCursor0.y}, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f}); 
//                 bool bFreeNode = false;

//                 for(uint32_t j = 0; j < uNodesPerBlock; j++)
//                 {
//                     plDeviceAllocationNode* ptNode = &sbtNodes[uNodesPerBlock * i + j];

//                     if(ptNode->ulSizeWasted == ptNode->ulSize)
//                         continue;

//                     float fStart = fWidthAvailable * ((float) ptNode->ulOffset) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
                    
//                     if(ptNode->ulSizeWasted > ptNode->ulSize)
//                     {
//                         float fFreeWidth = fWidthAvailable * ((float)ptNode->ulSize) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
//                         if(tMousePos.x > tCursor0.x + fStart && tMousePos.x < tCursor0.x + fStart + fFreeWidth)
//                         uHoveredNode = (uint32_t)ptNode->uNodeIndex;
//                         bFreeNode = true;
//                         continue;
//                     }

//                     float fUsedWidth = fWidthAvailable * ((float)ptNode->ulSize - ptNode->ulSizeWasted) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
                    
//                     pl_add_rect_filled(ptFgLayer, (plVec2){tCursor0.x + fStart, tCursor0.y}, (plVec2){tCursor0.x + fStart + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f}); 

//                     if(ptNode->ulSizeWasted > 0)
//                     {
                        
//                         const float fWastedWidth = fWidthAvailable * ((float)ptNode->ulSizeWasted) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
//                         const float fWasteStart = fStart + fUsedWidth;
//                         pl_add_rect_filled(ptFgLayer, (plVec2){tCursor0.x + fWasteStart, tCursor0.y}, (plVec2){tCursor0.x + fWasteStart + fWastedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.703f, 0.234f, 1.0f}); 
//                         if(tMousePos.x > tCursor0.x + fStart && tMousePos.x < tCursor0.x + fWasteStart + fWastedWidth)
//                             uHoveredNode = (uint32_t)ptNode->uNodeIndex;
//                     }
//                     else if(tMousePos.x > tCursor0.x + fStart && tMousePos.x < tCursor0.x + fStart + fUsedWidth)
//                         uHoveredNode = (uint32_t)ptNode->uNodeIndex;
//                 }

//                 if(pl_was_last_item_hovered())
//                 {
//                     pl_begin_tooltip();
//                     pl_text(sbDebugNames[uHoveredNode]);
//                     pl_text("Total Size:  %u", sbtNodes[uHoveredNode].ulSize);
//                     pl_text("Size Used:   %u", bFreeNode ? 0 : sbtNodes[uHoveredNode].ulSize - sbtNodes[uHoveredNode].ulSizeWasted);
//                     pl_text("Size Wasted: %u", bFreeNode ? 0 : sbtNodes[uHoveredNode].ulSizeWasted);
//                     pl_text("Offset:      %u", sbtNodes[uHoveredNode].ulOffset);
//                     pl_text("Memory Type: %u", sbtNodes[uHoveredNode].uMemoryType);
//                     pl_end_tooltip();
//                 }
//             }

//         }

//         sbtBlocks = ptDevice->tLocalDedicatedAllocator.blocks(ptDevice->tLocalDedicatedAllocator.ptInst, &uBlockCount);
//         if(uBlockCount > 0)
//         {

//             pl_layout_dynamic(0.0f, 1);
//             pl_separator();
//             pl_text("Device Memory: Local Dedicated");

//             pl_layout_template_begin(30.0f);
//             pl_layout_template_push_static(150.0f);
//             pl_layout_template_push_variable(300.0f);
//             pl_layout_template_end();

//             static const uint64_t ulMaxBlockSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE;

//             for(uint32_t i = 0; i < uBlockCount; i++)
//             {
//                 plDeviceAllocationBlock* ptBlock = &sbtBlocks[i];
//                 char* pcTempBuffer0 = pl_temp_allocator_sprintf(&tTempAllocator, "Block %u: %0.1fMB###d", i, ((double)ptBlock->ulSize)/1000000.0);
//                 char* pcTempBuffer1 = pl_temp_allocator_sprintf(&tTempAllocator, "Block %u###d", i);

//                 pl_button(pcTempBuffer0);
                
//                 plVec2 tCursor0 = pl_get_cursor_pos();
//                 const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
//                 float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)ulMaxBlockSize;
//                 float fUsedWidth = fWidthAvailable * ((float)ptBlock->sbtRanges[0].tAllocation.ulSize) / (float)ulMaxBlockSize;

//                 if(fUsedWidth < 10.0f)
//                     fUsedWidth = 10.0f;
//                 else if (fUsedWidth > 500.0f)
//                     fUsedWidth = 525.0f;
                    
//                 if (fTotalWidth > 500.0f)
//                     fTotalWidth = 525.0f;
                
//                 pl_invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
//                 if(pl_was_last_item_active())
//                 {
//                     pl_add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.703f, 0.234f, 1.0f});
//                     pl_add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f});
//                     pl_add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  fTotalWidth, 30.0f + tCursor0.y}, (plVec4){ 1.f, 1.0f, 1.0f, 1.0f}, 2.0f);
//                 }
//                 else
//                 {
//                     pl_add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.703f, 0.234f, 1.0f});
//                     pl_add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f});
//                 }

//                 if(pl_was_last_item_hovered())
//                 {
//                     pl_begin_tooltip();
//                     pl_text(ptBlock->sbtRanges[0].pcName);
//                     pl_end_tooltip();
//                 }

//                 pl_temp_allocator_reset(&tTempAllocator);
//             }
//         }

//         pl_end_window();
//     }
// }

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

    if(pl_begin_window("Logging", bValue, false))
    {
        const plVec2 tWindowSize = pl_get_window_size();
        const plVec2 tWindowPos = pl_get_window_pos();
        const plVec2 tWindowEnd = pl_add_vec2(tWindowSize, tWindowPos);

        static bool bActiveLevels[6] = { true, true, true, true, true, true};

        pl_layout_dynamic(0.0f, 6);
        pl_checkbox("Trace", &bActiveLevels[0]);
        pl_checkbox("Debug", &bActiveLevels[1]);
        pl_checkbox("Info", &bActiveLevels[2]);
        pl_checkbox("Warn", &bActiveLevels[3]);
        pl_checkbox("Error", &bActiveLevels[4]);
        pl_checkbox("Fatal", &bActiveLevels[5]);

        bool bUseClipper = true;
        for(uint32_t i = 0; i < 6; i++)
        {
            if(bActiveLevels[i] == false)
            {
                bUseClipper = false;
                break;
            }
        }

        pl_layout_dynamic(0.0f, 1);
        if(pl_begin_tab_bar("tab bar"))
        {
            uint32_t uChannelCount = 0;
            plLogChannel* ptChannels = pl_get_log_channels(&uChannelCount);
            for(uint32_t i = 0; i < uChannelCount; i++)
            {
                plLogChannel* ptChannel = &ptChannels[i];
                
                if(ptChannel->tType & PL_CHANNEL_TYPE_CYCLIC_BUFFER)
                {
                    if(pl_begin_tab(ptChannel->pcName))
                    {
                        const plVec2 tCursorPos = pl_get_cursor_pos();
                        pl_layout_dynamic(tWindowEnd.y - tCursorPos.y - 20.0f, 1);
                        if(pl_begin_child(ptChannel->pcName))
                        {
                            pl_layout_dynamic(0.0f, 1);
                            const uint64_t uIndexStart = ptChannel->uEntryCount;
                            const uint64_t uLogCount = pl_min(PL_LOG_CYCLIC_BUFFER_SIZE, ptChannel->uEntryCount);

                            
                            if(bUseClipper)
                            {
                                plUiClipper tClipper = {(uint32_t)uLogCount};
                                while(pl_step_clipper(&tClipper))
                                {
                                    for(uint32_t j = tClipper.uDisplayStart; j < tClipper.uDisplayEnd; j++)
                                    {
                                        uint32_t uActualIndex0 = (uIndexStart + j) % (uint32_t)uLogCount;
                                        const plLogEntry* ptEntry = &ptChannel->atEntries[uActualIndex0];
                                        pl_color_text(atColors[ptEntry->uLevel / 1000 - 5], &ptChannel->pcBuffer0[ptEntry->uOffset + ptChannel->uBufferCapacity * (ptEntry->uGeneration % 2)]);
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
                                            pl_color_text(atColors[ptEntry->uLevel / 1000 - 5], &ptChannel->pcBuffer0[ptEntry->uOffset + ptChannel->uBufferCapacity * (ptEntry->uGeneration % 2)]);
                                    }  
                            }
                            pl_end_child();
                        }
                        pl_end_tab();
                    }
                }
                else if(ptChannel->tType & PL_CHANNEL_TYPE_BUFFER)
                {
                    if(pl_begin_tab(ptChannel->pcName))
                    {
                        const plVec2 tCursorPos = pl_get_cursor_pos();
                        pl_layout_dynamic(tWindowEnd.y - tCursorPos.y - 20.0f, 1);
                        if(pl_begin_child(ptChannel->pcName))
                        {
                            pl_layout_dynamic(0.0f, 1);

                            if(bUseClipper)
                            {
                                plUiClipper tClipper = {(uint32_t)ptChannel->uEntryCount};
                                while(pl_step_clipper(&tClipper))
                                {
                                    for(uint32_t j = tClipper.uDisplayStart; j < tClipper.uDisplayEnd; j++)
                                    {
                                        plLogEntry* ptEntry = &ptChannel->pEntries[j];
                                        pl_color_text(atColors[ptEntry->uLevel / 1000 - 5], &ptChannel->pcBuffer0[ptEntry->uOffset]);
                                    } 
                                }
                            }
                            else
                            {
                                for(uint32_t j = 0; j < ptChannel->uEntryCount; j++)
                                {
                                    plLogEntry* ptEntry = &ptChannel->pEntries[j];
                                    if(bActiveLevels[ptEntry->uLevel / 1000 - 5])
                                        pl_color_text(atColors[ptEntry->uLevel / 1000 - 5], &ptChannel->pcBuffer0[ptEntry->uOffset]);
                                } 
                            }
                            pl_end_child();
                        }
                        pl_end_tab();
                    }
                }
            }

            pl_end_tab_bar();
        }
        pl_end_window();
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
    ptMemoryCtx = ptDataRegistry->get_data(PL_CONTEXT_MEMORY);
    pl_set_memory_context(ptMemoryCtx);
    pl_set_profile_context(ptDataRegistry->get_data("profile"));
    pl_set_log_context(ptDataRegistry->get_data("log"));
    pl_set_ui_context(ptDataRegistry->get_data("ui"));

    ptStatsApi = ptApiRegistry->first(PL_API_STATS);
    ptIOCtx = pl_get_io();

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
    pl_sb_free(sbppdValues);
    pl_sb_free(sbppdFrameValues);
    pl_sb_free(sbdRawValues);
    pl_sb_free(sbbValues);
    pl_temp_allocator_free(&tTempAllocator);
}
