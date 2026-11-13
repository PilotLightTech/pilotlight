/*
   pl_tools_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] internal api
// [SECTION] internal api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

// core
#include <float.h>
#include "pl.h"

#include "pl_tools_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extra
#include "pl_memory.h"

// extensions
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"
#include "pl_stats_ext.h"
#include "pl_graphics_ext.h"
#include "pl_gpu_allocators_ext.h"
#include "pl_profile_ext.h"
#include "pl_log_ext.h"
#include "pl_console_ext.h"
#include "pl_asset_ext.h"
#include "pl_material_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#endif

#ifndef PL_ICON_FA_FILTER
    #define PL_ICON_FA_FILTER "\xef\x82\xb0"	// U+f0b0
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plDebugContext
{
    // other
    plDevice*       ptDevice;
    plTempAllocator tTempAllocator;

    // stat data
    double**       sbppdValues;
    const char**   ppcNames;
    double***      sbppdFrameValues; // values to write to
    double*        sbdRawValues; // raw values
    bool*          sbbValues;
    uint32_t       uSelectedCount;
    uint32_t       uMaxSelectedCount;
    float          fStatSelectorWidth;
    plUiTextFilter tStatFilter;

    // profile data
    plProfileCpuSample* sbtSamples;
    float fDeltaTime;
    bool bProfileFirstRun;

    // memory data
    size_t             szLastFreedAmount;
    plAllocationEntry* sbtActiveAllocations;
    plUiTextFilter     tMemoryFilter;

    // options
    bool bShowDeviceMemoryAnalyzer;
    bool bShowMemoryAllocations;
    bool bShowProfiling;
    bool bShowGpuProfiling;
    bool bGpuProfileOpen;
    bool bGpuProfilePaused;
    plGpuTimingFrame tGpuFrame;
    plGpuTimingSample* sbtGpuSamples;
    plProfileCpuSample* sbtGpuGraphSamples;
    bool bShowStats;
    bool bShowLogging;
} plDebugContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plDebugContext* gptDebugCtx = NULL;

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

    static const plDataRegistryI*  gptDataRegistry  = NULL;
    static const plGraphicsI*      gptGfx           = NULL;
    static const plStatsI*         gptStats         = NULL;
    static const plGPUAllocatorsI* gptGpuAllocators = NULL;
    static const plDrawI*          gptDraw          = NULL;
    static const plUiI*            gptUI            = NULL;
    static const plIOI*            gptIOI           = NULL;
    static const plProfileI*       gptProfile       = NULL;
    static const plLogI*           gptLog           = NULL;
    static const plConsoleI*       gptConsole       = NULL;

    static plIO* gptIO = NULL;

#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static void pl__show_memory_allocations(bool* bValue);
static void pl__show_profiling         (bool* bValue);
static void pl__show_gpu_profiling     (bool* bValue);
static void pl__show_statistics        (bool* bValue);
static void pl__show_device_memory     (bool* bValue);
static void pl__show_logging           (bool* bValue);

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

void
pl_tools_initialize(plToolsInit tInit)
{
    gptDebugCtx->ptDevice = tInit.ptDevice;
    if(gptConsole->add_bool_variable)
    {
        gptConsole->add_toggle_variable("t.LogTool", &gptDebugCtx->bShowLogging, "shows log tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
        gptConsole->add_toggle_variable("t.StatTool", &gptDebugCtx->bShowStats, "shows stats tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
        gptConsole->add_toggle_variable("t.ProfileTool", &gptDebugCtx->bShowProfiling, "shows profiling tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
        gptConsole->add_toggle_variable("t.GPUProfileTool", &gptDebugCtx->bShowGpuProfiling, "shows GPU timing tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
        gptConsole->add_toggle_variable("t.MemoryAllocationTool", &gptDebugCtx->bShowMemoryAllocations, "shows memory tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
        gptConsole->add_toggle_variable("t.DeviceMemoryAnalyzerTool", &gptDebugCtx->bShowDeviceMemoryAnalyzer, "shows gpu memory tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
    }
}

void
pl_tools_update(void)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    if(gptDebugCtx->bShowMemoryAllocations)
    {
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "Memory Allocations");
        pl__show_memory_allocations(&gptDebugCtx->bShowMemoryAllocations);
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    }

    if(gptDebugCtx->bShowProfiling)
    {
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "Profiling");
        pl__show_profiling(&gptDebugCtx->bShowProfiling);
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    }

    if(gptDebugCtx->bShowGpuProfiling && gptDebugCtx->ptDevice)
    {
        if(!gptDebugCtx->bGpuProfileOpen)
            gptGfx->set_gpu_timing_enabled(gptDebugCtx->ptDevice, true);
        gptDebugCtx->bGpuProfileOpen = true;
        pl__show_gpu_profiling(&gptDebugCtx->bShowGpuProfiling);
    }
    if(gptDebugCtx->bGpuProfileOpen && !gptDebugCtx->bShowGpuProfiling)
        gptGfx->set_gpu_timing_enabled(gptDebugCtx->ptDevice, false);
    gptDebugCtx->bGpuProfileOpen = gptDebugCtx->bShowGpuProfiling;

    if(gptDebugCtx->bShowStats)
    {
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "Statistics");
        pl__show_statistics(&gptDebugCtx->bShowStats);
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    }

    if(gptDebugCtx->bShowDeviceMemoryAnalyzer)
    {
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "Device Memory Analyzer");
        pl__show_device_memory(&gptDebugCtx->bShowDeviceMemoryAnalyzer);
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    }

    if(gptDebugCtx->bShowLogging)
    {
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "Logging");
        pl__show_logging(&gptDebugCtx->bShowLogging);
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__show_memory_allocations(bool* bValue)
{    
    const size_t szHostMemoryInUse = gptGfx->get_host_memory_in_use();
    const size_t szLocalMemoryInUse = gptGfx->get_local_memory_in_use();
    const size_t szMemoryUsage = gptMemory->get_memory_usage();
    const size_t szActiveAllocations = gptMemory->get_allocation_count();
    const size_t szAllocationFrees = gptMemory->get_free_count();

    if(szAllocationFrees != gptDebugCtx->szLastFreedAmount)
    {

    }

    if(gptUI->begin_window("Memory Allocations", bValue, false))
    {
        gptUI->layout_dynamic(0.0f, 1);
        if(szMemoryUsage > 1000000000)
            gptUI->labeled_text("General Memory Usage", "%0.3f gb", (double)szMemoryUsage / 1000000000);
        else if(szMemoryUsage > 1000000)
            gptUI->labeled_text("General Memory Usage", "%0.3f mb", (double)szMemoryUsage / 1000000);
        else if(szMemoryUsage > 1000)
            gptUI->labeled_text("General Memory Usage", "%0.3f kb", (double)szMemoryUsage / 1000);
        else
            gptUI->labeled_text("General Memory Usage", "%llu bytes", (double)szMemoryUsage);
    
        if(szHostMemoryInUse > 1000000000)
            gptUI->labeled_text("Host Graphics Memory Usage", "%0.3f gb", (double)szHostMemoryInUse / 1000000000);
        else if(szHostMemoryInUse > 1000000)
            gptUI->labeled_text("Host Graphics Memory Usage", "%0.3f mb", (double)szHostMemoryInUse / 1000000);
        else if(szHostMemoryInUse > 1000)
            gptUI->labeled_text("Host Graphics Memory Usage", "%0.3f kb", (double)szHostMemoryInUse / 1000);
        else
            gptUI->labeled_text("Host Graphics Memory Usage", "%llu bytes", (double)szHostMemoryInUse);

        gptUI->labeled_text("Active Allocations", "%u", szActiveAllocations);
        gptUI->labeled_text("Freed Allocations", "%u", szAllocationFrees);

        if(gptUI->input_text_hint("File Filter", "Filter (inc,-exc)", gptDebugCtx->tMemoryFilter.acInputBuffer, 256, 0))
        {
            gptUI->text_filter_build(&gptDebugCtx->tMemoryFilter);
        }

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

        if(szAllocationFrees != gptDebugCtx->szLastFreedAmount)
        {
            pl_sb_reset(gptDebugCtx->sbtActiveAllocations);
            pl_sb_reserve(gptDebugCtx->sbtActiveAllocations, gptMemory->get_allocation_count() + 1);
            size_t szOriginalAllocationCount = 0;
            plAllocationEntry* atAllocations = gptMemory->get_allocations(&szOriginalAllocationCount);

            for(uint32_t i = 0; i < szOriginalAllocationCount; i++)
            {
                if(atAllocations[i].szSize > 0)
                {
                    pl_sb_push(gptDebugCtx->sbtActiveAllocations, atAllocations[i]);
                }
            }
            gptDebugCtx->szLastFreedAmount = szAllocationFrees;
        }

        gptUI->layout_dynamic(gptUI->get_window_size().y - (gptUI->get_cursor_pos().y - gptUI->get_window_pos().y) - 15.0f, 1);
        if(gptUI->begin_child("allocation child", 0, 0))
        {
            gptUI->layout_template_begin(30.0f);
            gptUI->layout_template_push_static(50.0f);
            gptUI->layout_template_push_variable(100.0f);
            gptUI->layout_template_push_variable(50.0f);
            gptUI->layout_template_push_variable(50.0f);
            gptUI->layout_template_end();

            const uint32_t uEntryCount = pl_sb_size(gptDebugCtx->sbtActiveAllocations);

            if(gptUI->text_filter_active(&gptDebugCtx->tMemoryFilter))
            {
                for(uint32_t i = 0; i < uEntryCount; i++)
                {
                    size_t szUnused = 0;
                    plAllocationEntry tEntry = gptDebugCtx->sbtActiveAllocations[i];
                    if(gptUI->text_filter_pass(&gptDebugCtx->tMemoryFilter, tEntry.pcFileOnly, NULL))
                    {
                        strncpy(pcFile, tEntry.pcFileOnly, 1024);
                        gptUI->text("%i", i);
                        gptUI->text("%s", pcFile);
                        gptUI->text("%i", tEntry.iLine);
                        gptUI->text("%u", tEntry.szSize);
                    }
                } 
            }
            else
            {
                plUiClipper tClipper = {uEntryCount};
                while(gptUI->step_clipper(&tClipper))
                {
                    for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                    {
                        size_t szUnused = 0;
                        plAllocationEntry tEntry = gptDebugCtx->sbtActiveAllocations[i];
                        strncpy(pcFile, tEntry.pcFileOnly, 1024);
                        gptUI->text("%i", i);
                        gptUI->text("%s", pcFile);
                        gptUI->text("%i", tEntry.iLine);
                        gptUI->text("%u", tEntry.szSize);
                    } 
                }
            }
            gptUI->end_child();
        }
        gptUI->end_window();
    }
}

// Keep navigation in seconds. Converting a huge scrolling canvas to floats
// loses precision when inspecting a short sample late in a frame.
static void
pl__profile_clamp_view(double dFrameDuration, double* pdStart, double* pdSpan)
{
    const double dMinSpan = pl_mind(dFrameDuration, pl_maxd(1e-9, dFrameDuration * DBL_EPSILON * 32.0));
    *pdSpan = pl_clampd(dMinSpan, *pdSpan, dFrameDuration);
    *pdStart = pl_clampd(0.0, *pdStart, dFrameDuration - *pdSpan);
}

static void
pl__profile_zoom_view(double dFrameDuration, double dRatio, double dWheel, double* pdStart, double* pdSpan)
{
    dRatio = pl_clampd(0.0, dRatio, 1.0);
    const double dAnchor = *pdStart + dRatio * *pdSpan;
    *pdSpan *= pow(1.2, -dWheel);
    pl__profile_clamp_view(dFrameDuration, pdStart, pdSpan);
    *pdStart = dAnchor - dRatio * *pdSpan;
    pl__profile_clamp_view(dFrameDuration, pdStart, pdSpan);
}

static double
pl__profile_tick_step(double dSpan, float fWidth, float fLabelWidth)
{
    const double dTarget = dSpan * pl_maxd(1.0, fLabelWidth) / pl_maxd(1.0, fWidth);
    const double dMagnitude = pow(10.0, floor(log10(dTarget)));
    const double dFraction = dTarget / dMagnitude;
    double dStep = 10.0;
    if(dFraction <= 1.0)      dStep = 1.0;
    else if(dFraction <= 2.0) dStep = 2.0;
    else if(dFraction <= 5.0) dStep = 5.0;
    return dStep * dMagnitude;
}

typedef struct _plProfileGraphState
{
    double dViewStart;
    double dViewSpan;
    float fDepthScroll;
    bool bFitGraph;
    bool bGraphDragged;
} plProfileGraphState;

static void
pl__profile_graph(const plProfileCpuSample* ptSamples, uint32_t uSampleSize, double dFrameDuration, plProfileGraphState* ptState, const char* pcSpanLabel)
{
    static const plVec4 atColors[6] = {
        {0.0f, 1.0f, 1.0f, 0.75f},
        {1.0f, 0.5f, 0.0f, 0.75f},
        {0.0f, 1.0f, 0.0f, 0.75f},
        {0.0f, 0.5f, 1.0f, 0.75f},
        {1.0f, 1.0f, 0.0f, 0.75f},
        {1.0f, 0.0f, 1.0f, 0.75}
    };
    const plVec2 tWindowEnd = pl_add_vec2(gptUI->get_window_pos(), gptUI->get_window_size());
    gptUI->layout_static(0.0f, 110.0f, 1);
    if(gptUI->button("Fit Frame")) ptState->bFitGraph = true;
    gptUI->layout_dynamic(0.0f, 1);
    gptUI->text("Wheel: zoom at cursor | Drag: pan time/depth | Click sample: focus");
    const plVec2 tParentCursorPos = gptUI->get_cursor_pos();
    gptUI->layout_dynamic(pl_maxf(1.0f, tWindowEnd.y - tParentCursorPos.y - 15.0f), 1);
    if(gptUI->begin_child("timeline", 0, PL_UI_WINDOW_FLAGS_NO_SCROLLBAR))
    {
        const plVec2 tChildPos = gptUI->get_window_pos();
        const plVec2 tChildSize = gptUI->get_window_size();
        const plVec2 tCursorPos = gptUI->get_cursor_pos();
        const float fWidth = pl_maxf(1.0f, tChildPos.x + tChildSize.x - tCursorPos.x - 10.0f);
        const float fHeight = pl_maxf(1.0f, tChildPos.y + tChildSize.y - tCursorPos.y - 10.0f);
        const float fRowsTop = tCursorPos.y + 50.0f;
        const float fBottom = tCursorPos.y + fHeight;
        uint32_t uMaxDepth = 0;
        for(uint32_t uIndex = 0; uIndex < uSampleSize; uIndex++)
            uMaxDepth = pl_max(uMaxDepth, ptSamples[uIndex]._uDepth);
        const float fMaxDepthScroll = pl_maxf(0.0f, (uMaxDepth + 1) * 25.0f - pl_maxf(0.0f, fHeight - 50.0f));
        if(ptState->bFitGraph)
        {
            ptState->dViewStart = 0.0;
            ptState->dViewSpan = dFrameDuration;
            ptState->fDepthScroll = 0.0f;
            ptState->bFitGraph = false;
        }
        pl__profile_clamp_view(dFrameDuration, &ptState->dViewStart, &ptState->dViewSpan);
        ptState->fDepthScroll = pl_clampf(0.0f, ptState->fDepthScroll, fMaxDepthScroll);

        gptUI->layout_static(fHeight, fWidth, 1);
        const bool bClicked = gptUI->invisible_button("##profile_canvas", (plVec2){fWidth, fHeight});
        const bool bHovered = gptUI->was_last_item_hovered();
        const bool bActive = gptUI->was_last_item_active();
        const plVec2 tMousePos = gptIOI->get_mouse_pos();
        if(bHovered && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
            ptState->bGraphDragged = false;
        if(bHovered && gptIOI->get_mouse_wheel() != 0.0f)
            pl__profile_zoom_view(dFrameDuration, (tMousePos.x - tCursorPos.x) / fWidth, gptIOI->get_mouse_wheel(), &ptState->dViewStart, &ptState->dViewSpan);
        if(bActive && gptIOI->is_mouse_down(PL_MOUSE_BUTTON_LEFT) && (ptState->bGraphDragged || gptIOI->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 5.0f)))
        {
            ptState->bGraphDragged = true;
            const plVec2 tDrag = gptIOI->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 0.0f);
            ptState->dViewStart -= (double)tDrag.x * ptState->dViewSpan / (double)fWidth;
            ptState->fDepthScroll = pl_clampf(0.0f, ptState->fDepthScroll - tDrag.y, fMaxDepthScroll);
            pl__profile_clamp_view(dFrameDuration, &ptState->dViewStart, &ptState->dViewSpan);
            gptIOI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }

        const double dPixelsPerSecond = (double)fWidth / ptState->dViewSpan;
        const double dViewEnd = ptState->dViewStart + ptState->dViewSpan;
        plDrawLayer2D* ptFgLayer = gptUI->get_window_fg_drawlayer();
        plFont* ptFont = gptUI->get_default_font();
        gptDraw->add_rect_filled(ptFgLayer, tCursorPos, (plVec2){tCursorPos.x + fWidth, pl_minf(fRowsTop, fBottom)},
            (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.5f, 0.0f, 0.0f, 0.7f)});

        // Choose 1/2/5 ticks from pixel spacing. Labels use stack buffers;
        // no pointers survive resetting or growing a temporary allocator.
        double dIncrement = pl__profile_tick_step(ptState->dViewSpan, fWidth, 100.0f);
        double dUnit = 1.0;
        const char* pcUnit = "s";
        uint32_t uDecimals = 0;
        char acLabel[96];
        for(uint32_t uPass = 0; uPass < 2; uPass++)
        {
            dUnit = 1.0;
            pcUnit = "s";
            if(dIncrement < 1.0)  { dUnit = 1e3; pcUnit = "ms"; }
            if(dIncrement < 1e-3) { dUnit = 1e6; pcUnit = "us"; }
            if(dIncrement < 1e-6) { dUnit = 1e9; pcUnit = "ns"; }
            uDecimals = (uint32_t)pl_clampd(0.0, ceil(-log10(dIncrement * dUnit)), 6.0);
            pl_snprintf(acLabel, sizeof(acLabel), "%.*f %s", (int)uDecimals, dViewEnd * dUnit, pcUnit);
            const float fLabelWidth = gptDraw->calculate_text_size(acLabel, (plDrawTextOptions){.ptFont = ptFont}).x + 20.0f;
            if(uPass == 0)
                dIncrement = pl__profile_tick_step(ptState->dViewSpan, fWidth, pl_maxf(100.0f, fLabelWidth));
        }
        const double dFirstTick = floor(ptState->dViewStart / dIncrement) * dIncrement;
        const uint32_t uTickCount = (uint32_t)ceil(ptState->dViewSpan / dIncrement) + 2;
        float fLastLabelEnd = tCursorPos.x - 8.0f;
        for(uint32_t uTick = 0; uTick < uTickCount; uTick++)
        {
            const double dTime = dFirstTick + (double)uTick * dIncrement;
            for(uint32_t uMinor = 0; uMinor < 5; uMinor++)
            {
                const double dTickTime = dTime + (double)uMinor * dIncrement / 5.0;
                if(dTickTime < ptState->dViewStart || dTickTime > dViewEnd)
                    continue;
                const float fX = tCursorPos.x + (float)((dTickTime - ptState->dViewStart) * dPixelsPerSecond);
                const float fTickHeight = uMinor == 0 ? 18.0f : 6.0f;
                gptDraw->add_line(ptFgLayer, (plVec2){fX, tCursorPos.y}, (plVec2){fX, pl_minf(tCursorPos.y + fTickHeight, fBottom)}, (plDrawLineOptions){.uColor = PL_COLOR_32_WHITE, .fThickness = 1.0f});
                if(uMinor != 0) continue;
                pl_snprintf(acLabel, sizeof(acLabel), "%.*f %s", (int)uDecimals, dTickTime * dUnit, pcUnit);
                const float fTextWidth = gptDraw->calculate_text_size(acLabel, (plDrawTextOptions){.ptFont = ptFont}).x;
                const float fTextX = pl_clampf(tCursorPos.x, fX - fTextWidth * 0.5f, tCursorPos.x + pl_maxf(0.0f, fWidth - fTextWidth));
                if(fTextX < fLastLabelEnd + 8.0f)
                    continue;
                fLastLabelEnd = fTextX + fTextWidth;
                gptDraw->add_text(ptFgLayer, (plVec2){fTextX, tCursorPos.y + 20.0f}, (plDrawTextOptions){.ptFont = ptFont, .uColor = PL_COLOR_32_WHITE, .tClipMin = tCursorPos, .tClipMax = {tCursorPos.x + fWidth, fBottom}}, "%s", acLabel);
            }
        }

        uint32_t uHoveredSample = UINT32_MAX;
        for(uint32_t uIndex = 0; uIndex < uSampleSize; uIndex++)
        {
            const plProfileCpuSample* ptSample = &ptSamples[uIndex];
            const double dEnd = ptSample->dStartTime + pl_maxd(0.0, ptSample->dDuration);
            if(dEnd < ptState->dViewStart || ptSample->dStartTime > dViewEnd)
                continue;
            const float fY = fRowsTop + (float)ptSample->_uDepth * 25.0f - ptState->fDepthScroll;
            if(fY + 20.0f <= fRowsTop || fY >= fBottom)
                continue;
            const float fX = tCursorPos.x + (float)((pl_maxd(ptState->dViewStart, ptSample->dStartTime) - ptState->dViewStart) * dPixelsPerSecond);
            const float fEndX = tCursorPos.x + (float)((pl_mind(dViewEnd, dEnd) - ptState->dViewStart) * dPixelsPerSecond);

            // Very short samples remain visible/hittable without changing their times.
            const plVec2 tMin = {pl_minf(fX, tCursorPos.x + fWidth - 1.0f), pl_maxf(fRowsTop, fY)};
            const plVec2 tMax = {pl_minf(tCursorPos.x + fWidth, pl_maxf(fEndX, tMin.x + 1.0f)), pl_minf(fBottom, fY + 20.0f)};
            const plRect tRect = {tMin, tMax};
            gptDraw->add_rect_filled(ptFgLayer, tMin, tMax, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(atColors[ptSample->_uDepth % 6])});
            if(tMax.x - tMin.x > 8.0f)
                gptDraw->add_text(ptFgLayer, (plVec2){tMin.x + 3.0f, fY + 2.0f}, (plDrawTextOptions){.ptFont = ptFont, .uColor = PL_COLOR_32_WHITE, .tClipMin = tMin, .tClipMax = tMax}, "%s", ptSample->pcName);
            if(bHovered && pl_rect_contains_point(&tRect, tMousePos))
                uHoveredSample = uIndex;
        }
        if(bHovered)
        {
            const double dHoveredTime = ptState->dViewStart + (double)(tMousePos.x - tCursorPos.x) / dPixelsPerSecond;
            gptDraw->add_line(ptFgLayer, (plVec2){tMousePos.x, tCursorPos.y}, (plVec2){tMousePos.x, fBottom}, (plDrawLineOptions){.uColor = PL_COLOR_32_WHITE, .fThickness = 1.0f});
            if(uHoveredSample == UINT32_MAX)
            {
                gptUI->begin_tooltip();
                gptUI->text("%.3f %s", dHoveredTime * dUnit, pcUnit);
                gptUI->end_tooltip();
            }
        }
        if(uHoveredSample != UINT32_MAX)
        {
            const plProfileCpuSample* ptSample = &ptSamples[uHoveredSample];
            gptUI->begin_tooltip();
            gptUI->color_text(atColors[ptSample->_uDepth % 6], "%s", ptSample->pcName);
            gptUI->text("Duration: %.3f us", ptSample->dDuration * 1e6);
            gptUI->text("Start: %.3f us", ptSample->dStartTime * 1e6);
            gptUI->text("%s: %.2f %%", pcSpanLabel, 100.0 * ptSample->dDuration / dFrameDuration);
            gptUI->end_tooltip();
            if(bClicked && !ptState->bGraphDragged && gptIOI->is_mouse_released(PL_MOUSE_BUTTON_LEFT))
            {
                ptState->dViewSpan = pl_maxd(1e-9, ptSample->dDuration * 1.2);
                ptState->dViewStart = ptSample->dStartTime + ptSample->dDuration * 0.5 - ptState->dViewSpan * 0.5;
                pl__profile_clamp_view(dFrameDuration, &ptState->dViewStart, &ptState->dViewSpan);
            }
        }
        gptUI->end_child();
    }
}

static void
pl__show_profiling(bool* bValue)
{
    static plProfileGraphState tGraph = {.dViewSpan = 0.016, .bFitGraph = true};
    static const plVec4 atColors[6] = {
        {0.0f, 1.0f, 1.0f, 0.75f},
        {1.0f, 0.5f, 0.0f, 0.75f},
        {0.0f, 1.0f, 0.0f, 0.75f},
        {0.0f, 0.5f, 1.0f, 0.75f},
        {1.0f, 1.0f, 0.0f, 0.75f},
        {1.0f, 0.0f, 1.0f, 0.75}
    };

    gptUI->set_next_window_size((plVec2){750, 700}, PL_UI_COND_ONCE);
    if(gptUI->begin_window("Profiling (WIP)", bValue, 0))
    {
        plProfileCpuSample* ptSamples = NULL;
        uint32_t uSampleSize = 0;
        if(gptDebugCtx->bProfileFirstRun && gptIO->ulFrameCount == 1)
        {
            ptSamples = gptProfile->get_last_frame_samples(0, &uSampleSize);
            pl_sb_resize(gptDebugCtx->sbtSamples, uSampleSize);
            memcpy(gptDebugCtx->sbtSamples, ptSamples, sizeof(plProfileCpuSample) * uSampleSize);
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
            ptSamples = gptProfile->get_last_frame_samples(0, &uSampleSize);
            gptDebugCtx->fDeltaTime = gptIO->fDeltaTime;
        }

        gptUI->layout_static(0.0f, 150.0f, 2);
        if(pl_sb_size(gptDebugCtx->sbtSamples) == 0)
        {
            if(gptUI->button("Capture Frame"))
            {
                tGraph.bFitGraph = true;
                pl_sb_resize(gptDebugCtx->sbtSamples, uSampleSize);
                memcpy(gptDebugCtx->sbtSamples, ptSamples, sizeof(plProfileCpuSample) * uSampleSize);
            }
        }
        else
        {
            if(gptUI->button("Release Frame"))
            {
                tGraph.bFitGraph = true;
                pl_sb_reset(gptDebugCtx->sbtSamples);
            }
        }
        double dFrameDuration = pl_maxd(1e-9, (double)gptDebugCtx->fDeltaTime);
        for(uint32_t uIndex = 0; uIndex < uSampleSize; uIndex++)
            dFrameDuration = pl_maxd(dFrameDuration, ptSamples[uIndex].dStartTime + ptSamples[uIndex].dDuration);
        gptUI->text("Frame Time: %.3f ms", dFrameDuration * 1000.0);

        gptUI->layout_dynamic(0.0f, 1);

        gptUI->separator();

        if(gptUI->begin_tab_bar("profiling tabs", 0))
        {
            if(gptUI->begin_tab("Table", 0))
            {
                gptUI->layout_dynamic(0.0f, 1);
                static double dProfilerOverhead = 0.0f;
                if(pl_sb_size(gptDebugCtx->sbtSamples) == 0)
                    dProfilerOverhead = gptProfile->get_last_frame_overhead(0) * 1000.0;
                gptUI->text("Profiler Overhead: %0.6f ms", dProfilerOverhead);

                gptUI->layout_template_begin(0.0f);
                gptUI->layout_template_push_variable(400.0f);
                gptUI->layout_template_push_static(75.0f);
                gptUI->layout_template_push_static(95.0f);
                gptUI->layout_template_push_variable(100.0f);
                gptUI->layout_template_end();

                gptUI->text("Sample Name");
                gptUI->text("Time (ms)");
                gptUI->text("Start (ms)");
                gptUI->text("Frame Time");

                gptUI->layout_dynamic(0.0f, 1);
                gptUI->separator();

                gptUI->layout_template_begin(0.0f);
                gptUI->layout_template_push_variable(400.0f);
                gptUI->layout_template_push_static(75.0f);
                gptUI->layout_template_push_static(95.0f);
                gptUI->layout_template_push_variable(100.0f);
                gptUI->layout_template_end();

                plUiClipper tClipper = {uSampleSize};

                while(gptUI->step_clipper(&tClipper))
                {
                    for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                    {
                        gptUI->indent(15.0f * (float)(ptSamples[i]._uDepth + 1));
                        gptUI->color_text(atColors[ptSamples[i]._uDepth % 6], "%s", ptSamples[i].pcName);
                        gptUI->unindent(15.0f * (float)(ptSamples[i]._uDepth + 1));
                        gptUI->text("%7.3f", ptSamples[i].dDuration * 1000.0);
                        gptUI->text("%7.3f", ptSamples[i].dStartTime * 1000.0);
                        gptUI->push_theme_color(PL_UI_COLOR_PROGRESS_BAR, atColors[ptSamples[i]._uDepth % 6]);
                        gptUI->progress_bar((float)(ptSamples[i].dDuration / dFrameDuration), (plVec2){-1.0f, 0.0f}, NULL);
                        gptUI->pop_theme_color(1);
                    } 
                }
                gptUI->end_tab();
            }

            if(gptUI->begin_tab("Graph", 0))
            {
                pl__profile_graph(ptSamples, uSampleSize, dFrameDuration, &tGraph, "Frame Time");
                gptUI->end_tab();
            }
            gptUI->end_tab_bar();
        }
        gptUI->end_window();
    }
}

static void
pl__show_gpu_profiling(bool* bValue)
{
    static plProfileGraphState tGraph = {.dViewSpan = 0.016, .bFitGraph = true};
    const plGpuTimingFrame* ptLatest = gptGfx->get_gpu_timings(gptDebugCtx->ptDevice);
    gptUI->set_next_window_size((plVec2){800, 650}, PL_UI_COND_ONCE);
    if(gptUI->begin_window("GPU Profiling", bValue, 0))
    {
        gptUI->layout_dynamic(0.0f, 1);
        bool bEnabled = ptLatest->bEnabled;
        if(gptUI->checkbox("Collect timings", &bEnabled))
            gptGfx->set_gpu_timing_enabled(gptDebugCtx->ptDevice, bEnabled);
        if(!ptLatest->bSupported)
            gptUI->text("Timestamp queries are unavailable on this device.");
        else
        {
            gptUI->text("%s | Completed frames | Inclusive scope times", ptLatest->bCpu ? "CPU backend execution" : gptGfx->get_backend_string());
            gptUI->text("Collection adds profiling overhead. Scope times can overlap.");
            if(!gptDebugCtx->bGpuProfilePaused)
            {
                if(!gptDebugCtx->tGpuFrame.uSampleCount && ptLatest->uSampleCount)
                    tGraph.bFitGraph = true;
                gptDebugCtx->tGpuFrame = *ptLatest;
                pl_sb_resize(gptDebugCtx->sbtGpuSamples, ptLatest->uSampleCount);
                if(ptLatest->uSampleCount)
                    memcpy(gptDebugCtx->sbtGpuSamples, ptLatest->ptSamples, ptLatest->uSampleCount * sizeof(plGpuTimingSample));
                gptDebugCtx->tGpuFrame.ptSamples = gptDebugCtx->sbtGpuSamples;
            }
            const plGpuTimingFrame* ptFrame = &gptDebugCtx->tGpuFrame;
            gptUI->layout_static(0.0f, 140.0f, 2);
            if(gptUI->button(gptDebugCtx->bGpuProfilePaused ? "Release Frame" : "Capture Frame"))
            {
                gptDebugCtx->bGpuProfilePaused = !gptDebugCtx->bGpuProfilePaused;
                tGraph.bFitGraph = true;
            }
            gptUI->text("Frame: %llu", (unsigned long long)ptFrame->ulFrameIndex);
            gptUI->layout_dynamic(0.0f, 1);
            gptUI->text("Scope span: %.3f ms | Samples: %u | Dropped: %u", ptFrame->dDuration * 1000.0, ptFrame->uSampleCount, ptFrame->uDroppedSamples);
            if(!ptFrame->ulFrameIndex)
                gptUI->text("Waiting for a completed timing frame...");
            const double dDuration = pl_maxd(1e-9, ptFrame->dDuration);
            if(gptUI->begin_tab_bar("GPU profiling tabs", 0))
            {
                if(gptUI->begin_tab("Table", 0))
                {
                    gptUI->layout_template_begin(0.0f);
                    gptUI->layout_template_push_variable(280.0f);
                    gptUI->layout_template_push_static(95.0f);
                    gptUI->layout_template_push_static(95.0f);
                    gptUI->layout_template_push_variable(90.0f);
                    gptUI->layout_template_end();
                    gptUI->text("Sample");
                    gptUI->text("Time (ms)");
                    gptUI->text("Start (ms)");
                    gptUI->text("Scope span");
                    plUiClipper tClipper = {ptFrame->uSampleCount};
                    while(gptUI->step_clipper(&tClipper))
                    {
                        for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                        {
                            const plGpuTimingSample* ptSample = &ptFrame->ptSamples[i];
                            const float fIndent = 15.0f * ptSample->uDepth;
                            gptUI->indent(fIndent);
                            gptUI->text("%s", ptSample->acName);
                            gptUI->unindent(fIndent);
                            gptUI->text("%.6f", ptSample->dDuration * 1000.0);
                            gptUI->text("%.6f", ptSample->dStartTime * 1000.0);
                            gptUI->progress_bar((float)(ptSample->dDuration / dDuration), (plVec2){-1.0f, 0.0f}, NULL);
                        }
                    }
                    gptUI->end_tab();
                }
                if(gptUI->begin_tab("Graph", 0))
                {
                    pl_sb_resize(gptDebugCtx->sbtGpuGraphSamples, ptFrame->uSampleCount);
                    for(uint32_t i = 0; i < ptFrame->uSampleCount; i++)
                    {
                        const plGpuTimingSample* ptSample = &ptFrame->ptSamples[i];
                        gptDebugCtx->sbtGpuGraphSamples[i] = (plProfileCpuSample){
                            .pcName = ptSample->acName,
                            .dStartTime = ptSample->dStartTime,
                            .dDuration = ptSample->dDuration,
                            ._uDepth = ptSample->uDepth
                        };
                    }
                    pl__profile_graph(gptDebugCtx->sbtGpuGraphSamples, ptFrame->uSampleCount, dDuration, &tGraph, "Scope span");
                    gptUI->end_tab();
                }
                gptUI->end_tab_bar();
            }
        }
        gptUI->end_window();
    }
}

static void
pl__show_statistics(bool* bValue)
{

    static const plVec4 atColors[6] = {
        {0.0f, 1.0f, 1.0f, 0.75f},
        {1.0f, 0.5f, 0.0f, 0.75f},
        {0.0f, 1.0f, 0.0f, 0.75f},
        {0.0f, 0.5f, 1.0f, 0.75f},
        {1.0f, 1.0f, 0.0f, 0.75f},
        {1.0f, 0.0f, 1.0f, 0.75}
    };

    const char** apcTempNames = NULL;
    plVec4* atSelectedColors = NULL;
    const uint32_t uStatsMaxFrames = gptStats->get_max_frames();

    // selectable values

    uint32_t uNameCount = 0;
    gptDebugCtx->ppcNames = gptStats->get_names(&uNameCount);

    if(gptDebugCtx->sbbValues == NULL) // first run
    {

        pl_sb_resize(gptDebugCtx->sbbValues, uNameCount);
        pl_sb_resize(gptDebugCtx->sbppdValues, uNameCount);
        pl_sb_resize(gptDebugCtx->sbppdFrameValues, uNameCount);
        gptDebugCtx->sbbValues[0] = true;
        gptDebugCtx->uSelectedCount = 1;
        gptDebugCtx->uMaxSelectedCount = gptDebugCtx->uSelectedCount;
        pl_sb_resize(gptDebugCtx->sbdRawValues, uStatsMaxFrames *  gptDebugCtx->uMaxSelectedCount);
        *gptStats->get_counter_data(gptDebugCtx->ppcNames[0]) = gptDebugCtx->sbppdValues[0];
        for(uint32_t i = 0; i < uNameCount; i++)
        {
            gptDebugCtx->sbppdFrameValues[i] = gptStats->get_counter_data(gptDebugCtx->ppcNames[i]);
            float fCurrentWidth = gptDraw->calculate_text_size(gptDebugCtx->ppcNames[i], (plDrawTextOptions){.ptFont = gptUI->get_default_font()}).x;
            if(fCurrentWidth > gptDebugCtx->fStatSelectorWidth)
                gptDebugCtx->fStatSelectorWidth = fCurrentWidth * 1.1f;
        }
    }
    
    if(uNameCount != pl_sb_size(gptDebugCtx->sbbValues))
    {

        pl_sb_resize(gptDebugCtx->sbbValues, uNameCount);
        pl_sb_resize(gptDebugCtx->sbppdValues, uNameCount);
        pl_sb_resize(gptDebugCtx->sbppdFrameValues, uNameCount);
        gptDebugCtx->uMaxSelectedCount = uNameCount;
        pl_sb_resize(gptDebugCtx->sbdRawValues, uStatsMaxFrames *  gptDebugCtx->uMaxSelectedCount);
        for(uint32_t i = 0; i < uNameCount; i++)
        {
            gptDebugCtx->sbppdFrameValues[i] = gptStats->get_counter_data(gptDebugCtx->ppcNames[i]);
            float fCurrentWidth = gptDraw->calculate_text_size(gptDebugCtx->ppcNames[i], (plDrawTextOptions){.ptFont = gptUI->get_default_font()}).x;
            if(fCurrentWidth > gptDebugCtx->fStatSelectorWidth)
                gptDebugCtx->fStatSelectorWidth = fCurrentWidth * 1.1f;
        }
    }

    gptUI->set_next_window_size((plVec2){900.0f, 450.0f}, PL_UI_COND_ONCE);
    if(gptUI->begin_window("Statistics", bValue, false))
    {
        static bool bAllowNegative = false;
        gptUI->text("Frame rate: %.0f FPS", gptIO->fFrameRate);
        gptUI->text("Frame time: %.6f s", gptIO->fDeltaTime);
        if(gptUI->input_text_hint("Stat Filter", "Filter (inc,-exc)", gptDebugCtx->tStatFilter.acInputBuffer, sizeof(gptDebugCtx->tStatFilter.acInputBuffer), 0))
            gptUI->text_filter_build(&gptDebugCtx->tStatFilter);
        const plVec2 tCursor = gptUI->get_cursor_pos();

        plDrawLayer2D* ptFgLayer = gptUI->get_window_fg_drawlayer();
        const plVec2 tWindowPos = gptUI->get_window_pos();
        const plVec2 tWindowSize = pl_sub_vec2(gptUI->get_window_size(), pl_sub_vec2(tCursor, tWindowPos));
        
        const plVec2 tWindowEnd = pl_add_vec2(gptUI->get_window_size(), tWindowPos);

        gptUI->layout_template_begin(tWindowSize.y - 15.0f);
        gptUI->layout_template_push_static(pl_minf(gptDebugCtx->fStatSelectorWidth, tWindowSize.x * 0.45f));
        gptUI->layout_template_push_dynamic();
        gptUI->layout_template_end();
      
        if(gptUI->begin_child("left", 0, 0))
        {
            gptUI->layout_dynamic(0.0f, 1);
 
            for(uint32_t i = 0; i < pl_sb_size(gptDebugCtx->sbppdValues); i++)
            {
                if(!gptUI->text_filter_pass(&gptDebugCtx->tStatFilter, gptDebugCtx->ppcNames[i], NULL))
                    continue;
                gptUI->push_theme_color(PL_UI_COLOR_HEADER, atColors[i % 6]);
                if(gptUI->selectable(gptDebugCtx->ppcNames[i], &gptDebugCtx->sbbValues[i], 0))
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
                gptUI->pop_theme_color(1);
            }
            gptUI->end_child();
        }

        uint32_t uSelectionSlot = 0;
        apcTempNames = pl_temp_allocator_alloc(&gptDebugCtx->tTempAllocator, sizeof(const char*) * gptDebugCtx->uSelectedCount);
        atSelectedColors = pl_temp_allocator_alloc(&gptDebugCtx->tTempAllocator, sizeof(plVec4) * gptDebugCtx->uSelectedCount);
        for(uint32_t i = 0; i < pl_sb_size(gptDebugCtx->sbbValues); i++)
        {
            if(gptDebugCtx->sbbValues[i])
            {
                apcTempNames[uSelectionSlot] = gptDebugCtx->ppcNames[i];
                atSelectedColors[uSelectionSlot] = atColors[i % 6];
                *gptDebugCtx->sbppdFrameValues[i] = &gptDebugCtx->sbdRawValues[uSelectionSlot * uStatsMaxFrames];
                uSelectionSlot++;
            }
            else
            {
                *gptDebugCtx->sbppdFrameValues[i] = NULL;
            }
        }

        
        if(gptUI->begin_tab_bar("stat tabs", 0))
        {
            if(gptUI->begin_tab("Plot", 0))
            {
                
                gptUI->layout_dynamic(tWindowSize.y - 40.0f, 1);
                const plVec2 tCursor0 = gptUI->get_cursor_pos();
                const plVec2 tPlotSize = {pl_maxf(1.0f, tWindowEnd.x - tCursor0.x - 20.0f), pl_maxf(1.0f, tWindowSize.y - 40.0f)};
                gptUI->invisible_button("plot", tPlotSize);

                gptDraw->add_rect_rounded_filled(ptFgLayer, 
                    tCursor0, 
                    pl_add_vec2(tCursor0, tPlotSize),
                    0.0f, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.2f, 0.0f, 0.0f, 0.5f)});
       
                const double dAxisMultiplier = bAllowNegative ? 2.0 : 1.0;

                // const double dYCenter = tCursor0.y + tPlotSize.y * 0.5f;            
                const double dYCenter = tCursor0.y + tPlotSize.y / dAxisMultiplier;    

                if(bAllowNegative)
                {
                    gptDraw->add_text(ptFgLayer, (plVec2){roundf(tCursor0.x), roundf((float)dYCenter)}, (plDrawTextOptions){.uColor = PL_COLOR_32_WHITE, .ptFont = gptUI->get_default_font()}, "0");
                    gptDraw->add_line(ptFgLayer, (plVec2){tCursor0.x, (float)dYCenter}, (plVec2){tCursor0.x + tPlotSize.x, (float)dYCenter}, (plDrawLineOptions){.uColor = PL_COLOR_32_WHITE, .fThickness = 1.0f});
                }

                bAllowNegative = false;
                for(uint32_t i = 0; i < gptDebugCtx->uSelectedCount; i++)
                {
                    const plVec4* ptColor = &atSelectedColors[i];
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

                    for(uint32_t j = 0; j < uStatsMaxFrames - 1; j++)
                    {
                        uint32_t uActualIndex0 = (uIndexStart + j) % uStatsMaxFrames;
                        uint32_t uActualIndex1 = (uIndexStart + j + 1) % uStatsMaxFrames;
                        const plVec2 tLineStart = {tCursor0.x + (float)(j * dXIncrement), (float)(dYCenter - dValues[uActualIndex0] * dConversion)};
                        const plVec2 tLineEnd = {tCursor0.x + (float)((j + 1) * dXIncrement), (float)(dYCenter - dValues[uActualIndex1] * dConversion)};
                        gptDraw->add_line(ptFgLayer, tLineStart, tLineEnd, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(*ptColor), .fThickness = 1.0f});

                        if(j == uStatsMaxFrames - 2)
                        {
                            gptDraw->add_text(ptFgLayer, (plVec2){roundf(tLineEnd.x), roundf(tLineEnd.y) - 6.0f}, (plDrawTextOptions){.uColor = PL_COLOR_32_VEC4(*ptColor), .ptFont = gptUI->get_default_font()}, "%0.0f", dValues[uActualIndex1]);
                        }
                    }
                    
                }
                gptUI->end_tab();
            }

            if(gptUI->begin_tab("Table", 0))
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
                    gptDraw->add_line(ptFgLayer, (plVec2){fXPos, fYPos}, (plVec2){fXPos, 3000.0f}, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.7f, 0.0f, 0.0f), .fThickness = 1.0f});
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
    static const plVec4 tTextColor      = {1.0f, 1.0f, 1.0f, 1.0f};

    const size_t szHostMemoryInUse = gptGfx->get_host_memory_in_use();
    const size_t szLocalMemoryInUse = gptGfx->get_local_memory_in_use();

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

        gptUI->layout_dynamic(0.0f, 1);

        if(gptUI->begin_tab_bar("Memory Tabs", 0))
        {
            if(gptUI->begin_tab("Allocators", 0))
            {
                const plDeviceMemoryAllocatorI atAllocators[] = {
                    *gptGpuAllocators->get_local_buddy_allocator(gptDebugCtx->ptDevice),
                    *gptGpuAllocators->get_local_dedicated_allocator(gptDebugCtx->ptDevice),
                    *gptGpuAllocators->get_staging_uncached_buddy_allocator(gptDebugCtx->ptDevice),
                    *gptGpuAllocators->get_staging_uncached_allocator(gptDebugCtx->ptDevice),
                    *gptGpuAllocators->get_staging_cached_allocator(gptDebugCtx->ptDevice)
                };

                const char* apcAllocatorNames[] = {
                    "Device Memory: Local Buddy",
                    "Device Memory: Local Dedicated",
                    "Device Memory: Staging Uncached Buddy",
                    "Device Memory: Staging Uncached",
                    "Device Memory: Staging Cached"
                };

                gptUI->push_theme_color(PL_UI_COLOR_BUTTON, tButtonColor);
                gptUI->push_theme_color(PL_UI_COLOR_BUTTON_ACTIVE, tButtonColor);
                gptUI->push_theme_color(PL_UI_COLOR_BUTTON_HOVERED, tButtonColor);
                for(uint32_t uAllocatorIndex = 0; uAllocatorIndex < 5; uAllocatorIndex++)
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

                        const uint64_t ulMaxBlockSize = gptGpuAllocators->get_buddy_block_size();

                        uint32_t iCurrentBlock = 0;

                        for(uint32_t i = 0; i < uBlockCount; i++)
                        {
                            plDeviceMemoryAllocation* ptBlock = &sbtBlocks[i];
                            // if(ptBlock->ulSize == 0)
                            //     continue;

                            char* pcTempBuffer1 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "Block %u##%u", iCurrentBlock, uAllocatorIndex);

                            char* pcTempBuffer0 = NULL;
                            if(ptBlock->ulSize > 1000000000)
                                pcTempBuffer0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "Block %u: %3.1fGB", iCurrentBlock, ((double)ptBlock->ulSize)/1000000000.0);
                            else if(ptBlock->ulSize > 1000000)
                                pcTempBuffer0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "Block %u: %3.1fMB", iCurrentBlock, ((double)ptBlock->ulSize)/1000000.0);
                            else if(ptBlock->ulSize > 1000)
                                pcTempBuffer0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "Block %u: %3.1fKB", iCurrentBlock, ((double)ptBlock->ulSize)/1000.0);
                            else
                                pcTempBuffer0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "Block %u: %luB", iCurrentBlock, ptBlock->ulSize);

                            gptUI->push_theme_color(PL_UI_COLOR_TEXT, tTextColor);
                            gptUI->button(pcTempBuffer0);
                            gptUI->pop_theme_color(1);
                            
                            plVec2 tCursor0 = gptUI->get_cursor_pos();

                            if(fHeight0 == -1.0f)
                            {
                                fWidth0  = tCursor0.x;
                                fHeight0 = tCursor0.y;
                            }

                            const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
                            float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)ulMaxBlockSize;
                            fTotalWidth = pl_max(fTotalWidth, 1.0f);

                            if(ptBlock->uHandle == UINT64_MAX)
                                gptDraw->add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth - 6.0f, 30.0f + tCursor0.y}, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tAvailableColor), .fThickness = 1.0f});
                            else
                                gptDraw->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tAvailableColor)});
                            gptUI->invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
                            if(gptUI->was_last_item_hovered())
                            {
                                ulHoveredBlock = (uint64_t)i;
                            }
                            pl_temp_allocator_reset(&gptDebugCtx->tTempAllocator);

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

                            const float fYPos = fHeight0 + 34.0f * (float)ptRange->ulBlockIndex;
                            gptDraw->add_rect_rounded_filled(ptFgLayer, (plVec2){fStartPos, fYPos}, (plVec2){fStartPos + fAvailableWidth, 30.0f + fYPos}, 0.0f, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tWastedColor)});
                            gptDraw->add_rect_rounded_filled(ptFgLayer, (plVec2){fStartPos, fYPos}, (plVec2){fStartPos + fUsedWidth, 30.0f + fYPos}, 0.0f, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tUsedColor)});

                            if(ptRange->ulBlockIndex == ulHoveredBlock)
                            {
                                const plRect tHitBox = pl_calculate_rect((plVec2){fStartPos, fYPos}, (plVec2){fAvailableWidth, 30});
                                if(pl_rect_contains_point(&tHitBox, tMousePos))
                                {

                                    char* pcTempBuffer0 = NULL;
                                    char* pcTempBuffer1 = NULL;


                                    if(ptRange->ulUsedSize > 1000000000)
                                        pcTempBuffer0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%0.1fGB ##%u", ((double)ptRange->ulUsedSize)/1000000000.0, i);
                                    else if(ptRange->ulUsedSize > 1000000)
                                        pcTempBuffer0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%0.1fMB ##%u", ((double)ptRange->ulUsedSize)/1000000.0, i);
                                    else if(ptRange->ulUsedSize > 1000)
                                        pcTempBuffer0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%0.1fKB ##%u", ((double)ptRange->ulUsedSize)/1000.0, i);
                                    else
                                        pcTempBuffer0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%lu ##%u", ptRange->ulUsedSize, i);

                                    if(ptRange->ulTotalSize > 1000000000)
                                        pcTempBuffer1 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%0.1fGB ##%u", ((double)ptRange->ulTotalSize)/1000000000.0, i);
                                    else if(ptRange->ulTotalSize > 1000000)
                                        pcTempBuffer1 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%0.1fMB ##%u", ((double)ptRange->ulTotalSize)/1000000.0, i);
                                    else if(ptRange->ulTotalSize > 1000)
                                        pcTempBuffer1 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%0.1fKB ##%u", ((double)ptRange->ulTotalSize)/1000.0, i);
                                    else
                                        pcTempBuffer1 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%lu ##%u", ptRange->ulTotalSize, i);


                                    gptDraw->add_rect(ptFgLayer, tHitBox.tMin, tHitBox.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tWhiteColor), .fThickness = 1.0f});
                                    gptUI->begin_tooltip();
                                    float fMaxWidth = gptDraw->calculate_text_size(ptRange->pcName, (plDrawTextOptions){.ptFont = gptUI->get_default_font()}).x + 25.0f;
                                    gptUI->layout_static(0.0f, pl_max(fMaxWidth, 300), 1);
                                    gptUI->text(ptRange->pcName);
                                    gptUI->text("Offset:          %lu", ptRange->ulOffset);
                                    gptUI->text("Requested Size:  %s", pcTempBuffer0);
                                    gptUI->text("Allocated Size:  %s", pcTempBuffer1);
                                    gptUI->text("Wasted:          %lu", ptRange->ulTotalSize - ptRange->ulUsedSize);
                                    gptUI->end_tooltip();

                                    pl_temp_allocator_reset(&gptDebugCtx->tTempAllocator);
                                }
                            }
                        }
                    }
                }

                gptUI->pop_theme_color(3);
                gptUI->end_tab();
            }

            if(gptUI->begin_tab("All Blocks", 0))
            {

                gptUI->layout_dynamic(0.0f, 2);
                
                static plMemoryFlags tFlagMasks = PL_MEMORY_FLAGS_DEVICE_LOCAL | PL_MEMORY_FLAGS_HOST_VISIBLE
                    | PL_MEMORY_FLAGS_HOST_COHERENT | PL_MEMORY_FLAGS_HOST_CACHED;

                gptUI->checkbox_flags("PL_MEMORY_FLAGS_DEVICE_LOCAL", &tFlagMasks, PL_MEMORY_FLAGS_DEVICE_LOCAL);
                gptUI->checkbox_flags("PL_MEMORY_FLAGS_HOST_VISIBLE", &tFlagMasks, PL_MEMORY_FLAGS_HOST_VISIBLE);
                gptUI->checkbox_flags("PL_MEMORY_FLAGS_HOST_COHERENT", &tFlagMasks, PL_MEMORY_FLAGS_HOST_COHERENT);
                gptUI->checkbox_flags("PL_MEMORY_FLAGS_HOST_CACHED", &tFlagMasks, PL_MEMORY_FLAGS_HOST_CACHED);

                gptUI->layout_dynamic(0.0f, 1);
                gptUI->separator_text("Blocks");

                plVec2 tCursorPos = gptUI->get_cursor_pos();

                gptUI->layout_dynamic(tWindowEnd.y - tCursorPos.y - 20.0f, 1);

                if(gptUI->begin_child("Memory Blocks", 0, 0))
                {

                    uint32_t uAllocationCount = 0;
                    const plDeviceMemoryAllocation* sbtAllocations = gptGfx->get_allocations(gptDebugCtx->ptDevice, &uAllocationCount);
                    tCursorPos = gptUI->get_cursor_pos();

                    const double dReferenceWidth = (double)gptUI->get_window_size().x - 50.0;
                    gptUI->layout_dynamic(tWindowEnd.y - tCursorPos.y - 20.0f, 1);

                    static const plVec4 tGPUColor = {0.5f, 0.5f, 0.0f, 1.0f};
                    static const plVec4 tCPUColor = {0.0f, 0.5f, 0.5f, 1.0f};

                    for(uint32_t i = 0; i < uAllocationCount; i++)
                    {

                        if(!(sbtAllocations[i].tMemoryFlags & tFlagMasks))
                            continue;

                        float fWidth = (float)(dReferenceWidth * ((double)sbtAllocations[i].ulSize) / (268435456.0 * 2.0));
                        fWidth = pl_maxf(fWidth, 70.0f);
                        gptUI->layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2);
                        gptUI->layout_row_push(fWidth);


                        if(sbtAllocations[i].tMemoryFlags & PL_MEMORY_FLAGS_DEVICE_LOCAL)
                        {
                            gptUI->push_theme_color(PL_UI_COLOR_BUTTON, tGPUColor);
                            gptUI->push_theme_color(PL_UI_COLOR_BUTTON_ACTIVE, tGPUColor);
                            gptUI->push_theme_color(PL_UI_COLOR_BUTTON_HOVERED, tGPUColor);
                        }
                        else
                        {
                            gptUI->push_theme_color(PL_UI_COLOR_BUTTON, tCPUColor);
                            gptUI->push_theme_color(PL_UI_COLOR_BUTTON_ACTIVE, tCPUColor);
                            gptUI->push_theme_color(PL_UI_COLOR_BUTTON_HOVERED, tCPUColor);
                        }

                        char* pcTempBuffer0 = NULL;
                        if(sbtAllocations[i].ulSize > 1000000000)
                            pcTempBuffer0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%0.1fGB ##%u", ((double)sbtAllocations[i].ulSize)/1000000000.0, i);
                        else if(sbtAllocations[i].ulSize > 1000000)
                            pcTempBuffer0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%0.1fMB ##%u", ((double)sbtAllocations[i].ulSize)/1000000.0, i);
                        else if(sbtAllocations[i].ulSize > 1000)
                            pcTempBuffer0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%0.1fKB ##%u", ((double)sbtAllocations[i].ulSize)/1000.0, i);
                        else
                            pcTempBuffer0 = pl_temp_allocator_sprintf(&gptDebugCtx->tTempAllocator, "%lu ##%u", sbtAllocations[i].ulSize, i);

                        gptUI->button(pcTempBuffer0);
                        gptUI->layout_row_push(200.0f);
                        gptUI->text("%s", sbtAllocations[i].acName);

                        if(gptUI->was_last_item_hovered())
                        {
                            gptUI->begin_tooltip();
                            gptUI->text(sbtAllocations[i].acName);
                            gptUI->text("Size: %s", pcTempBuffer0);
                            if(sbtAllocations[i].tMemoryFlags & PL_MEMORY_FLAGS_DEVICE_LOCAL) gptUI->text("PL_MEMORY_FLAGS_DEVICE_LOCAL");
                            if(sbtAllocations[i].tMemoryFlags & PL_MEMORY_FLAGS_HOST_VISIBLE) gptUI->text("PL_MEMORY_FLAGS_HOST_VISIBLE");
                            if(sbtAllocations[i].tMemoryFlags & PL_MEMORY_FLAGS_HOST_COHERENT) gptUI->text("PL_MEMORY_FLAGS_HOST_COHERENT");
                            if(sbtAllocations[i].tMemoryFlags & PL_MEMORY_FLAGS_HOST_CACHED) gptUI->text("PL_MEMORY_FLAGS_HOST_CACHED");
                            gptUI->end_tooltip();
                        }

                        gptUI->layout_row_end();
                
                        pl_temp_allocator_reset(&gptDebugCtx->tTempAllocator);

                        gptUI->pop_theme_color(3);
                    }
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
        if(gptUI->begin_tab_bar("tab bar", 0))
        {
            uint32_t uChannelCount = (uint32_t)gptLog->get_channel_count();
            for(uint32_t i = 0; i < uChannelCount; i++)
            {

                plLogExtChannelInfo tInfo = {0};
                bool bResult = gptLog->get_channel_info(i, &tInfo);

                uint64_t uEntryCount = tInfo.uEntryCount;
                plLogExtEntry* ptEntries = tInfo.ptEntries;

                if(tInfo.tType & PL_LOG_CHANNEL_TYPE_CYCLIC_BUFFER)
                {
                    if(gptUI->begin_tab(tInfo.pcName, 0))
                    {
                        gptUI->layout_static(0.0f, 50.0f, 1);
                        if(gptUI->button("reset"))
                        {
                            gptLog->reset_channel(i);
                            uEntryCount = 0;
                        }
                        const plVec2 tCursorPos = gptUI->get_cursor_pos();
                        gptUI->layout_dynamic(tWindowEnd.y - tCursorPos.y - 20.0f, 1);
                        if(gptUI->begin_child(tInfo.pcName, 0, 0))
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
                                        const plLogExtEntry* ptEntry = &ptEntries[uActualIndex0];
                                        gptUI->color_text(atColors[ptEntry->uLevel / 1000 - 5], &tInfo.pcBuffer[ptEntry->uOffset]);
                                    } 
                                }
                            }
                            else
                            {
                                    for(uint32_t j = i; j < uLogCount; j++)
                                    {
                                        uint32_t uActualIndex0 = (uIndexStart + j) % (uint32_t)uLogCount;
                                        const plLogExtEntry* ptEntry = &ptEntries[uActualIndex0];
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
                else if(tInfo.tType & PL_LOG_CHANNEL_TYPE_BUFFER)
                {
                    if(gptUI->begin_tab(tInfo.pcName, 0))
                    {
                        gptUI->layout_static(0.0f, 50.0f, 2);
                        if(gptUI->button("reset"))
                        {
                            gptLog->reset_channel(i);
                            uEntryCount = 0;
                        }
                        if(gptUI->button("clear"))
                        {
                            gptLog->clear_channel(i);
                            uEntryCount = 0;
                        }
                        const plVec2 tCursorPos = gptUI->get_cursor_pos();
                        gptUI->layout_dynamic(tWindowEnd.y - tCursorPos.y - 20.0f, 1);
                        if(gptUI->begin_child(tInfo.pcName, 0, 0))
                        {
                            gptUI->layout_dynamic(0.0f, 1);

                            if(bUseClipper)
                            {
                                plUiClipper tClipper = {(uint32_t)uEntryCount};
                                while(gptUI->step_clipper(&tClipper))
                                {
                                    for(uint32_t j = tClipper.uDisplayStart; j < tClipper.uDisplayEnd; j++)
                                    {
                                        plLogExtEntry* ptEntry = &ptEntries[j];
                                        gptUI->color_text(atColors[ptEntry->uLevel / 1000 - 5], &tInfo.pcBuffer[ptEntry->uOffset]);
                                    } 
                                }
                            }
                            else
                            {
                                for(uint32_t j = 0; j < uEntryCount; j++)
                                {
                                    plLogExtEntry* ptEntry = &ptEntries[j];
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

void
pl_load_tools_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    const plToolsI tApi = {
        .initialize = pl_tools_initialize,
        .update     = pl_tools_update
    };
    pl_set_api(ptApiRegistry, plToolsI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptDataRegistry  = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
        gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptIOI           = pl_get_api_latest(ptApiRegistry, plIOI);
        gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptGpuAllocators = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
        gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptUI            = pl_get_api_latest(ptApiRegistry, plUiI);
        gptProfile       = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptLog           = pl_get_api_latest(ptApiRegistry, plLogI);
        gptConsole       = pl_get_api_latest(ptApiRegistry, plConsoleI);
        gptIO = gptIOI->get_io();
    #endif

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

void
pl_unload_tools_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plToolsI* ptApi = pl_get_api_latest(ptApiRegistry, plToolsI);
    ptApiRegistry->remove_api(ptApi);

    gptUI->text_filter_cleanup(&gptDebugCtx->tMemoryFilter);
    gptUI->text_filter_cleanup(&gptDebugCtx->tStatFilter);
    pl_sb_free(gptDebugCtx->sbppdValues);
    pl_sb_free(gptDebugCtx->sbppdFrameValues);
    pl_sb_free(gptDebugCtx->sbdRawValues);
    pl_sb_free(gptDebugCtx->sbbValues);
    pl_sb_free(gptDebugCtx->sbtSamples);
    pl_sb_free(gptDebugCtx->sbtGpuSamples);
    pl_sb_free(gptDebugCtx->sbtGpuGraphSamples);
    pl_sb_free(gptDebugCtx->sbtActiveAllocations);
    pl_temp_allocator_free(&gptDebugCtx->tTempAllocator);
    gptDebugCtx = NULL;
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
