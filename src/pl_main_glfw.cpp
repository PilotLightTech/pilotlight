/*
   pl_main_glfw.c
     * glfw platform backend
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] globals
// [SECTION] forward declarations
// [SECTION] entry point
// [SECTION] helper implementations
// [SECTION] window ext
// [SECTION] library ext
// [SECTION] clipboard api
// [SECTION] threads ext
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h>
#include "pl_internal.h"
#include "pl_ds.h"

// platform specifics
#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
    #import <Cocoa/Cocoa.h>
    #import <Metal/Metal.h>
    #import <QuartzCore/CAMetalLayer.h>
    #include <time.h> // clock_gettime_nsec_np
    #include <sys/stat.h> // timespec
    #include <copyfile.h> // copyfile
    #include <dlfcn.h>    // dlopen, dlsym, dlclose
    #include <unistd.h> // close
    #include <fcntl.h> // O_RDONLY, O_WRONLY ,O_CREAT
    #include <pthread.h>
    #define GLFW_EXPOSE_NATIVE_COCOA
#else // linux
    #include <sys/sendfile.h> // sendfile
    #include <sys/stat.h> // stat, timespec
    #include <dlfcn.h> // dlopen, dlsym, dlclose
    #include <fcntl.h> // O_RDONLY, O_WRONLY ,O_CREAT
    #include <time.h> // clock_gettime, clock_getres
    #include <unistd.h> // usleep()
    #include <pthread.h>
    #define GLFW_EXPOSE_NATIVE_X11
#endif

// imgui
#include "imgui.h"
#include "imgui_internal.h" // ImLerp
#include "imgui_impl_glfw.h"

// glfw
#include <GLFW/glfw3.h>
#include "GLFW/glfw3native.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

#ifdef _WIN32
typedef struct _plSharedLibrary
{
    bool          bValid;
    uint32_t      uTempIndex;
    char          acPath[PL_MAX_PATH_LENGTH];
    char          acTransitionalName[PL_MAX_PATH_LENGTH];
    char          acLockFile[PL_MAX_PATH_LENGTH];
    plLibraryDesc tDesc;
    HMODULE       tHandle;
    FILETIME      tLastWriteTime;
} plSharedLibrary;

#elif defined(__APPLE__)

typedef struct _plSharedLibrary
{
    bool            bValid;
    uint32_t        uTempIndex;
    char            acPath[PL_MAX_PATH_LENGTH];
    char            acTransitionalName[PL_MAX_PATH_LENGTH];
    char            acLockFile[PL_MAX_PATH_LENGTH];
    plLibraryDesc   tDesc;
    void*           handle;
    struct timespec tLastWriteTime;
} plSharedLibrary;

#else // linux

typedef struct _plSharedLibrary
{
    bool          bValid;
    uint32_t      uTempIndex;
    char          acPath[PL_MAX_PATH_LENGTH];
    char          acTransitionalName[PL_MAX_PATH_LENGTH];
    char          acLockFile[PL_MAX_PATH_LENGTH];
    plLibraryDesc tDesc;
    void*         handle;
    time_t        tLastWriteTime;
} plSharedLibrary;

#endif

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

#ifdef _WIN32
bool  gbFirstRun                        = true;
bool  gbEnableVirtualTerminalProcessing = true;
INT64 ilTime                            = 0;
INT64 ilTicksPerSecond                  = 0;

#elif defined(__APPLE__)
id<MTLDevice>  device;
NSWindow*      nswin;
CFTimeInterval gtTime;
CAMetalLayer*  layer;
static inline CFTimeInterval pl__get_absolute_time(void) { return (CFTimeInterval)((double)(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1e9); }

#else // linux
double gdTime      = 0.0;
double gdFrequency = 0.0;

static inline double
pl__get_linux_absolute_time(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) 
    {
        PL_ASSERT(false && "clock_gettime() failed");
    }
    uint64_t nsec_count = ts.tv_nsec + ts.tv_sec * 1e9;
    return (double)nsec_count / gdFrequency;    
}
#endif

// glfw
GLFWwindow* ptGlfwWindow;
GLFWwindow* ptMouseWindow;
GLFWcursor* atMouseCursors[PL_MOUSE_CURSOR_COUNT];
bool        bMouseIgnoreButtonUpWaitForFocusLoss;
bool        bMouseIgnoreButtonUp;
plVec2      tLastValidMousePos;
GLFWwindow* atKeyOwnerWindows[GLFW_KEY_LAST];

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

void  pl_glfw_error_callback           (int error, const char* description);
void  pl_glfw_mouse_button_callback    (GLFWwindow*, int button, int action, int mods);
void  pl_glfw_mouse_pos_callback       (GLFWwindow*, double x, double y);
void  pl_glfw_cursor_enter_callback    (GLFWwindow*, int entered);
void  pl_glfw_window_focus_callback    (GLFWwindow*, int focused);
void  pl_glfw_scroll_callback          (GLFWwindow*, double xoffset, double yoffset);
void  pl_glfw_char_callback            (GLFWwindow*, unsigned int c);
void  pl_glfw_key_callback             (GLFWwindow*, int keycode, int scancode, int action, int mods);
void  pl_glfw_size_callback            (GLFWwindow*, int width, int height);
void  pl_glfw_window_iconified_callback(GLFWwindow*, int iconified);
void  pl_glfw_window_close_callback    (GLFWwindow*);
plKey pl_glfw_key_translate            (int keycode, int scancode);

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    const char* pcAppName = "editor";

    for(int i = 1; i < argc; i++)
    { 
        if(strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--app") == 0)
        {
            pcAppName = argv[i + 1];
            i++;
        }
        else if(strcmp(argv[i], "--version") == 0)
        {
            printf("\nPilot Light - light weight game engine\n\n");
            printf("Version: %s\n", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_CONFIG_DEBUG
                printf("Config: debug (glfw)\n\n");
            #endif
            #ifdef PL_CONFIG_RELEASE
                printf("Config: release (glfw)\n\n");
            #endif
            return 0;
        }
        else if(strcmp(argv[i], "--extensions") == 0)
        {
            plVersion tWindowExtVersion = plWindowI_version;
            plVersion tLibraryVersion = plLibraryI_version;
            printf("\nPilot Light - light weight game engine\n\n");
            printf("Version: %s\n", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_CONFIG_DEBUG
                printf("Config: debug\n\n");
            #endif
            #ifdef PL_CONFIG_RELEASE
                printf("Config: release\n\n");
            #endif
            return 0;
        }
        else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printf("\nPilot Light - light weight game engine\n");
            printf("Version: %s\n", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_CONFIG_DEBUG
                printf("Config: debug\n\n");
            #endif
            #ifdef PL_CONFIG_RELEASE
                printf("Config: release\n\n");
            #endif
            printf("Usage: pilot_light.exe [options]\n\n");
            printf("Options:\n");
            printf("-h              %s\n", "Displays this information.");
            printf("--help          %s\n", "Displays this information.");
            printf("--version       %s\n", "Displays Pilot Light version information.");
            printf("--extensions    %s\n", "Displays embedded extensions.");
            printf("-a <app>        %s\n", "Sets app to load. Default is 'editor'.");
            printf("--app <app>     %s\n", "Sets app to load. Default is 'editor'.");
            return 0;
        }
    }

    // setup timers
    #ifdef _WIN32
        QueryPerformanceFrequency((LARGE_INTEGER*)&ilTicksPerSecond);
        QueryPerformanceCounter((LARGE_INTEGER*)&ilTime);
    #elif defined(__APPLE__)
    #else // linux
        // setup timers
        static struct timespec ts;
        if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) 
        {
            PL_ASSERT(false && "clock_getres() failed");
        }
        gdFrequency = 1e9/((double)ts.tv_nsec + (double)ts.tv_sec * (double)1e9);
        gdTime = pl__get_linux_absolute_time();
    #endif

    // load core apis
    pl__load_core_apis();

    gptIOCtx = gptIOI->get_io();

    // clipboard
    gptIOCtx->set_clipboard_text_fn = [](void* pUnused, const char* text) { glfwSetClipboardString(nullptr, text); };
    gptIOCtx->get_clipboard_text_fn = [](void* pUnused) { return glfwGetClipboardString(nullptr); };

    // command line args
    gptIOCtx->iArgc = argc;
    gptIOCtx->apArgv = argv;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGuiContext* ptImGuiCtx = ImGui::CreateContext();

    ImGuiMemAllocFunc p_alloc_func = nullptr;
    ImGuiMemFreeFunc p_free_func = nullptr;
    void* p_user_data = nullptr;
    ImGui::GetAllocatorFunctions(&p_alloc_func, &p_free_func, &p_user_data);

    gptDataRegistry->set_data("imgui", ptImGuiCtx);
    gptDataRegistry->set_data("imgui allocate", (void*)p_alloc_func);
    gptDataRegistry->set_data("imgui free", (void*)p_free_func);
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // setup pilot light style
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Button]                 = ImVec4(0.51f, 0.02f, 0.10f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.61f, 0.02f, 0.10f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.87f, 0.02f, 0.10f, 1.00f);
    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.25f, 0.10f, 0.10f, 0.78f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.15f, 0.10f, 0.10f, 0.78f);
    colors[ImGuiCol_Border]                 = ImVec4(0.33f, 0.02f, 0.10f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.23f, 0.02f, 0.10f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.33f, 0.02f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.05f, 0.05f, 0.05f, 0.85f);
    colors[ImGuiCol_ScrollbarGrab]          = colors[ImGuiCol_Button];
    colors[ImGuiCol_ScrollbarGrabHovered]   = colors[ImGuiCol_ButtonHovered];
    colors[ImGuiCol_ScrollbarGrabActive]    = colors[ImGuiCol_ButtonActive];
    colors[ImGuiCol_CheckMark]              = colors[ImGuiCol_ButtonActive];
    colors[ImGuiCol_SliderGrab]             = colors[ImGuiCol_Button];
    colors[ImGuiCol_SliderGrabActive]       = colors[ImGuiCol_ButtonActive];
    colors[ImGuiCol_Header]                 = colors[ImGuiCol_Button];
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]              = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.98f, 0.59f, 0.26f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.98f, 0.59f, 0.26f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.98f, 0.59f, 0.26f, 0.95f);
    colors[ImGuiCol_InputTextCursor]        = colors[ImGuiCol_Text];
    colors[ImGuiCol_TabHovered]             = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_Tab]                    = ImLerp(colors[ImGuiCol_Header],       colors[ImGuiCol_TitleBgActive], 0.80f);
    colors[ImGuiCol_TabSelected]            = ImLerp(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.60f);
    colors[ImGuiCol_TabSelectedOverline]    = colors[ImGuiCol_HeaderActive];
    colors[ImGuiCol_TabDimmed]              = ImLerp(colors[ImGuiCol_Tab],          colors[ImGuiCol_TitleBg], 0.80f);
    colors[ImGuiCol_TabDimmedSelected]      = ImLerp(colors[ImGuiCol_TabSelected],  colors[ImGuiCol_TitleBg], 0.40f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.51f, 0.02f, 0.10f, 0.7f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);   // Prefer using Alpha=1.0 here
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);   // Prefer using Alpha=1.0 here
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextLink]               = colors[ImGuiCol_HeaderActive];
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavCursor]              = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    // when viewports are enabled we tweak WindowRounding/WindowBg so
    // platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    glfwSetErrorCallback(pl_glfw_error_callback);

    // setup glfw
    if (!glfwInit())
        return -1;

    atMouseCursors[PL_MOUSE_CURSOR_ARROW]      = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    atMouseCursors[PL_MOUSE_CURSOR_TEXT_INPUT] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    atMouseCursors[PL_MOUSE_CURSOR_RESIZE_NS]  = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    atMouseCursors[PL_MOUSE_CURSOR_RESIZE_EW]  = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    atMouseCursors[PL_MOUSE_CURSOR_HAND]       = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    // #if GLFW_HAS_NEW_CURSORS
        atMouseCursors[PL_MOUSE_CURSOR_RESIZE_ALL]  = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
        atMouseCursors[PL_MOUSE_CURSOR_RESIZE_NESW] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
        atMouseCursors[PL_MOUSE_CURSOR_RESIZE_NWSE] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
        atMouseCursors[PL_MOUSE_CURSOR_NOT_ALLOWED] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);
    // #else
    //     atMouseCursors[PL_MOUSE_CURSOR_RESIZE_ALL]  = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    //     atMouseCursors[PL_MOUSE_CURSOR_RESIZE_NESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    //     atMouseCursors[PL_MOUSE_CURSOR_RESIZE_NWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    //     atMouseCursors[PL_MOUSE_CURSOR_NOT_ALLOWED] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    // #endif

    // load app library
    const plLibraryI* ptLibraryApi = pl_get_api_latest(gptApiRegistry, plLibraryI);
    plLibraryDesc tLibraryDesc = {};
    tLibraryDesc.pcName = pcAppName;

    if(ptLibraryApi->load(tLibraryDesc, &gptAppLibrary))
    {
        #ifdef _WIN32
            pl_app_load     = (void* (__cdecl  *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__cdecl  *)(void*))            ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__cdecl  *)(plWindow*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__cdecl  *)(void*))            ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
            pl_app_info     = (bool  (__cdecl  *)(const plApiRegistryI*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_info");

        #else
            pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__attribute__(()) *)(plWindow*, void*))             ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
            pl_app_info     = (bool  (__attribute__(()) *)(const plApiRegistryI*))        ptLibraryApi->load_function(gptAppLibrary, "pl_app_info");
        #endif

        if(pl_app_info)
        {
            if(!pl_app_info(gptApiRegistry))
                return 0;
        }
        gpUserData = pl_app_load(gptApiRegistry, NULL);
        bool bApisFound = pl__check_apis();
        if(!bApisFound)
            return 3;
    }

    // main event loop
    while (gptIOCtx->bRunning)
    {

        glfwPollEvents();

        // reload library
        if(ptLibraryApi->has_changed(gptAppLibrary))
        {
            ptLibraryApi->reload(gptAppLibrary);
            #ifdef _WIN32
                pl_app_load     = (void* (__cdecl  *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
                pl_app_shutdown = (void  (__cdecl  *)(void*))            ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
                pl_app_resize   = (void  (__cdecl  *)(plWindow*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
                pl_app_update   = (void  (__cdecl  *)(void*))            ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
                pl_app_info     = (bool  (__cdecl  *)(const plApiRegistryI*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_info");

            #else
                pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
                pl_app_shutdown = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
                pl_app_resize   = (void  (__attribute__(()) *)(plWindow*, void*))             ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
                pl_app_update   = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
                pl_app_info     = (bool  (__attribute__(()) *)(const plApiRegistryI*))        ptLibraryApi->load_function(gptAppLibrary, "pl_app_info");
            #endif

            pl__handle_extension_reloads();
            gpUserData = pl_app_load(gptApiRegistry, gpUserData);
        }

        pl__garbage_collect_data_reg();

        // update time step
        #ifdef _WIN32
            INT64 ilCurrentTime = 0;
            QueryPerformanceCounter((LARGE_INTEGER*)&ilCurrentTime);
            gptIOCtx->fDeltaTime = (float)(ilCurrentTime - ilTime) / ilTicksPerSecond;
            ilTime = ilCurrentTime;
        #elif defined(__APPLE__)
            if(gtTime == 0.0)
                gtTime = pl__get_absolute_time();
            double dCurrentTime = pl__get_absolute_time();
            gptIOCtx->fDeltaTime = (float)(dCurrentTime - gtTime);
            gtTime = dCurrentTime;
        #else // linux
            const double dCurrentTime = pl__get_linux_absolute_time();
            gptIOCtx->fDeltaTime = (float)(dCurrentTime - gdTime);
            gdTime = dCurrentTime;
        #endif

        // start imgui glfw frame
        ImGui_ImplGlfw_NewFrame();

        // update mouse cursor
        plMouseCursor tCursor0 = gptIOCtx->tNextCursor;
        if(tCursor0 != PL_MOUSE_CURSOR_ARROW)
        {
            glfwSetCursor(ptGlfwWindow, atMouseCursors[tCursor0] ? atMouseCursors[tCursor0] : atMouseCursors[PL_MOUSE_CURSOR_ARROW]);
            glfwSetInputMode(ptGlfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        gptIOCtx->tNextCursor = PL_MOUSE_CURSOR_ARROW;
        gptIOCtx->bCursorChanged = false;

        // handle retina displays
        #ifdef __APPLE__

            float fCurrentScale = nswin.screen.backingScaleFactor;
            layer.contentsScale = fCurrentScale;

            int width, height;
            glfwGetFramebufferSize(ptGlfwWindow, &width, &height);
            layer.drawableSize = CGSizeMake(width, height);
            gptIOCtx->pBackendPlatformData = layer;

            // Setup display size (every frame to accommodate for window resizing)
            int w, h;
            glfwGetWindowSize(ptGlfwWindow, &w, &h);

            if (w > 0 && h > 0)
            {
                bool bResize = false;

                if(w != gptIOCtx->tMainViewportSize.x || h != gptIOCtx->tMainViewportSize.y)
                    bResize = true;
                else if(fCurrentScale != gptIOCtx->tMainFramebufferScale.x || fCurrentScale != gptIOCtx->tMainFramebufferScale.y )
                    bResize = true;

                if(bResize)
                {
                    gptIOCtx->tMainViewportSize.x = w;
                    gptIOCtx->tMainViewportSize.y = h;
                    gptIOCtx->tMainFramebufferScale.x = fCurrentScale;
                    gptIOCtx->tMainFramebufferScale.y = fCurrentScale;
                    pl_app_resize(gptMainWindow, gpUserData);
                }
            }

        #endif

        if(!gptIOCtx->bViewportMinimized)
        {
            pl_app_update(gpUserData);
            pl__handle_extension_reloads();
        }

        if (glfwGetWindowAttrib(ptGlfwWindow, GLFW_ICONIFIED) != 0)
        {
            #ifdef _WIN32
                ::Sleep(10);
            #else
                usleep(10 * 1000);
            #endif
            continue;
        }
    
        if(gbApisDirty)
            pl__check_apis();
    }

    // app cleanup
    pl_app_shutdown(gpUserData);

    // unload extensions & APIs
    pl__unload_all_extensions();
    pl__unload_core_apis();

    // cleanup app library
    if(gptAppLibrary)
    {
        PL_FREE(gptAppLibrary);
    }

    // check memory leaks
    pl__check_for_leaks();

    // shutdown imgui
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
}

//-----------------------------------------------------------------------------
// [SECTION] window ext
//-----------------------------------------------------------------------------

plWindowResult
pl_create_window(plWindowDesc tDesc, plWindow** pptWindowOut)
{

    plWindow* ptWindow = (plWindow*)malloc(sizeof(plWindow));
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    glfwWindowHint(GLFW_RESIZABLE, (tDesc.tFlags & PL_WINDOW_FLAG_RESIZABLE) ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, (tDesc.tFlags & PL_WINDOW_FLAG_DECORATED) ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING,  (tDesc.tFlags & PL_WINDOW_FLAG_TOP_MOST) ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_POSITION_X,  tDesc.iXPos);
    glfwWindowHint(GLFW_POSITION_Y,  tDesc.iXPos);

    ptGlfwWindow = glfwCreateWindow((int)tDesc.uWidth, (int)tDesc.uHeight, tDesc.pcTitle, NULL, NULL);

    int iMinWidth = tDesc.uMinWidth > 0 ? (int)tDesc.uMinWidth : GLFW_DONT_CARE;
    int iMaxWidth = tDesc.uMaxWidth > 0 ? (int)tDesc.uMaxWidth : GLFW_DONT_CARE;
    int iMinHeight = tDesc.uMinHeight > 0 ? (int)tDesc.uMinHeight : GLFW_DONT_CARE;
    int iMaxHeight = tDesc.uMaxHeight > 0 ? (int)tDesc.uMaxHeight : GLFW_DONT_CARE;
    glfwSetWindowSizeLimits(ptGlfwWindow, iMinWidth, iMinHeight, iMaxWidth, iMaxHeight);

    ptWindow->_pBackendData2 = ptGlfwWindow;

    if(gptMainWindow == nullptr)
        gptMainWindow = ptWindow;
    
    glfwSetMouseButtonCallback(ptGlfwWindow, pl_glfw_mouse_button_callback);
    glfwSetCursorPosCallback(ptGlfwWindow, pl_glfw_mouse_pos_callback);
    glfwSetCursorEnterCallback(ptGlfwWindow, pl_glfw_cursor_enter_callback);
    glfwSetScrollCallback(ptGlfwWindow, pl_glfw_scroll_callback);
    glfwSetCharCallback(ptGlfwWindow, pl_glfw_char_callback);
    glfwSetKeyCallback(ptGlfwWindow, pl_glfw_key_callback);
    glfwSetWindowFocusCallback(ptGlfwWindow, pl_glfw_window_focus_callback);
    glfwSetWindowSizeCallback(ptGlfwWindow, pl_glfw_size_callback);
    glfwSetWindowIconifyCallback(ptGlfwWindow, pl_glfw_window_iconified_callback);
    glfwSetWindowCloseCallback(ptGlfwWindow, pl_glfw_window_close_callback);

    #ifdef PL_METAL_BACKEND
    if(pl_sb_size(gsbtWindows) == 0)
        ImGui_ImplGlfw_InitForOther(ptGlfwWindow);
    #else
    if(pl_sb_size(gsbtWindows) == 0)
        ImGui_ImplGlfw_InitForVulkan(ptGlfwWindow);
    #endif

    #ifdef _WIN32
    HWND tHandle = glfwGetWin32Window(ptGlfwWindow);
    ptWindow->_pBackendData = tHandle;
    #elif defined(__APPLE__)
    device = MTLCreateSystemDefaultDevice();
    gptIOCtx->pBackendPlatformData = device;

    nswin = glfwGetCocoaWindow(ptGlfwWindow);
    layer = [CAMetalLayer layer];
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    nswin.contentView.layer = layer;
    nswin.contentView.wantsLayer = YES;

    typedef struct _plWindowData
    {
        void*           ptWindow;
        void* ptViewController;
        CAMetalLayer* ptLayer;
    } plWindowData;

    plWindowData* ptData = (plWindowData*)malloc(sizeof(plWindowData));
    ptData->ptLayer = layer;
    ptWindow->_pBackendData = ptData;

    #else // linux
    struct plPlatformData
    {
        uint32_t header;
        Display* dpy;
        Window window;
    };
    static plPlatformData tPlatformData = {UINT32_MAX};
    tPlatformData.dpy = glfwGetX11Display();
    tPlatformData.window = glfwGetX11Window(ptGlfwWindow);
    ptWindow->_pBackendData = &tPlatformData;
    #endif
    *pptWindowOut = ptWindow;
    pl_sb_push(gsbtWindows, ptWindow);
    return PL_WINDOW_RESULT_SUCCESS;
}

void
pl_destroy_window(plWindow* ptWindow)
{
    free(ptWindow);
}

void
pl_set_window_size(plWindow* ptWindow, uint32_t uWidth, uint32_t uHeight)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    glfwSetWindowSize(ptGlfwWindow, (int)uWidth, (int)uHeight);
}

void
pl_set_window_pos(plWindow* ptWindow, int iXPos, int iYPos)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    glfwSetWindowPos(ptGlfwWindow, iXPos, iYPos);
}

void
pl_get_window_size(plWindow* ptWindow, uint32_t* uWidth, uint32_t* uHeight)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    int iWidth = 0;
    int iHeight = 0;
    glfwGetWindowSize(ptGlfwWindow, &iWidth, &iHeight);
    *uWidth = (uint32_t)iWidth;
    *uHeight = (uint32_t)iHeight;
}

void
pl_get_window_pos(plWindow* ptWindow, int* iXPos, int* iYPos)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    glfwGetWindowPos(ptGlfwWindow, iXPos, iYPos);
}

void
pl_minimize_window(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    glfwIconifyWindow(ptGlfwWindow);
}

void
pl_show_window(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    glfwShowWindow(ptGlfwWindow);
}

void
pl_hide_window(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    glfwHideWindow(ptGlfwWindow);
}

void
pl_maximize_window(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    glfwMaximizeWindow(ptGlfwWindow);
}

void
pl_restore_window(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    glfwRestoreWindow(ptGlfwWindow);
}

void
pl_focus_window(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    glfwFocusWindow(ptGlfwWindow);
}

void
pl_hide_cursor(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    glfwSetInputMode(ptGlfwWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
}

void
pl_capture_cursor(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    glfwSetInputMode(ptGlfwWindow, GLFW_CURSOR, GLFW_CURSOR_CAPTURED);
}

void
pl_normal_cursor(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    glfwSetInputMode(ptGlfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void
pl_set_raw_mouse_input(plWindow* ptWindow, bool bValue)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(ptGlfwWindow, GLFW_RAW_MOUSE_MOTION, bValue ? GLFW_TRUE : GLFW_FALSE);
}

bool
pl_is_window_maximized(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    if (glfwGetWindowAttrib(ptGlfwWindow, GLFW_MAXIMIZED))
    {
        return true;
    }
    return false;
}

bool
pl_is_window_minimized(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    if (glfwGetWindowAttrib(ptGlfwWindow, GLFW_ICONIFIED))
    {
        return true;
    }
    return false;
}

bool
pl_is_window_focused(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    if (glfwGetWindowAttrib(ptGlfwWindow, GLFW_FOCUSED))
    {
        return true;
    }
    return false;
}

bool
pl_is_window_hovered(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    if (glfwGetWindowAttrib(ptGlfwWindow, GLFW_HOVERED))
    {
        return true;
    }
    return false;
}

bool
pl_is_window_resizable(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    if (glfwGetWindowAttrib(ptGlfwWindow, GLFW_RESIZABLE))
    {
        return true;
    }
    return false;
}

bool
pl_is_window_decorated(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    if (glfwGetWindowAttrib(ptGlfwWindow, GLFW_DECORATED))
    {
        return true;
    }
    return false;
}

bool
pl_is_window_top_most(plWindow* ptWindow)
{
    GLFWwindow* ptGlfwWindow = (GLFWwindow*)ptWindow->_pBackendData2;
    if (glfwGetWindowAttrib(ptGlfwWindow, GLFW_FLOATING))
    {
        return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
// [SECTION] threads ext
//-----------------------------------------------------------------------------

#ifdef _WIN32
typedef struct _plMutex
{
    HANDLE tHandle;
} plMutex;

void
pl_create_mutex(plMutex** ppMutexOut)
{
    HANDLE tHandle = CreateMutex(NULL, FALSE, NULL);
    if(tHandle)
    {
        (*ppMutexOut) = (plMutex*)malloc(sizeof(plMutex));
        (*ppMutexOut)->tHandle = tHandle;
    }
}

void
pl_destroy_mutex(plMutex** ptMutex)
{
    CloseHandle((*ptMutex)->tHandle);
    free((*ptMutex));
    (*ptMutex) = NULL;
}

void
pl_lock_mutex(plMutex* ptMutex)
{
    DWORD dwWaitResult = WaitForSingleObject(ptMutex->tHandle, INFINITE);
    PL_ASSERT(dwWaitResult == WAIT_OBJECT_0);
}

void
pl_unlock_mutex(plMutex* ptMutex)
{
    if(!ReleaseMutex(ptMutex->tHandle))
    {
        printf("ReleaseMutex error: %d\n", GetLastError());
        PL_ASSERT(false);
    }
}
#else // linux

typedef struct _plMutex
{
    pthread_mutex_t tHandle;
} plMutex;

void
pl_create_mutex(plMutex** pptMutexOut)
{
    *pptMutexOut = (plMutex*)malloc(sizeof(plMutex));
    if(pthread_mutex_init(&(*pptMutexOut)->tHandle, NULL)) //-V522
    {
        PL_ASSERT(false);
    }
}

void
pl_lock_mutex(plMutex* ptMutex)
{
    pthread_mutex_lock(&ptMutex->tHandle);
}

void
pl_unlock_mutex(plMutex* ptMutex)
{
    pthread_mutex_unlock(&ptMutex->tHandle);
}

void
pl_destroy_mutex(plMutex** pptMutex)
{
    pthread_mutex_destroy(&(*pptMutex)->tHandle);
    free((*pptMutex));
    *pptMutex = NULL;
}

#endif

//-----------------------------------------------------------------------------
// [SECTION] library ext
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// [SECTION] file ext
//-----------------------------------------------------------------------------

void
pl_binary_read_file(const char* pcFile, size_t* pszSizeIn, uint8_t* pcBuffer)
{
    if(pszSizeIn == NULL)
        return;

    FILE* ptDataFile = fopen(pcFile, "rb");
    size_t uSize = 0u;

    if (ptDataFile == NULL)
    {
        *pszSizeIn = 0u;
        return;
    }

    // obtain file size
    fseek(ptDataFile, 0, SEEK_END);
    uSize = ftell(ptDataFile);
    
    if(pcBuffer == NULL)
    {
        *pszSizeIn = uSize;
        fclose(ptDataFile);
        return;
    }
    fseek(ptDataFile, 0, SEEK_SET);

    // copy the file into the buffer:
    size_t szResult = fread(pcBuffer, sizeof(char), uSize, ptDataFile);
    if (szResult != uSize)
    {
        if (feof(ptDataFile))
            printf("Error reading test.bin: unexpected end of file\n");
        else if (ferror(ptDataFile)) {
            perror("Error reading test.bin");
        }
        return;
    }

    fclose(ptDataFile);
    return;
}



void
pl_copy_file(const char* source, const char* destination)
{
    #ifdef _WIN32
        BOOL bResult = CopyFile(source, destination, FALSE);
    #elif defined(__APPLE__)
    copyfile_state_t tS = copyfile_state_alloc();
    copyfile(source, destination, tS, COPYFILE_XATTR | COPYFILE_DATA);
    copyfile_state_free(tS);
    #else
        size_t bufferSize = 0u;
        pl_binary_read_file(source, &bufferSize, NULL);

        struct stat stat_buf;
        int fromfd = open(source, O_RDONLY);
        fstat(fromfd, &stat_buf);
        int tofd = open(destination, O_WRONLY | O_CREAT, stat_buf.st_mode);
        int n = 1;
        while (n > 0)
            n = sendfile(tofd, fromfd, 0, bufferSize * 2);
    #endif
}

//-----------------------------------------------------------------------------
// [SECTION] library ext
//-----------------------------------------------------------------------------

#ifdef _WIN32
static inline FILETIME
pl__get_last_write_time(const char* pcFilename)
{
    FILETIME tLastWriteTime = {0};
    
    WIN32_FILE_ATTRIBUTE_DATA tData = {0};
    if(GetFileAttributesExA(pcFilename, GetFileExInfoStandard, &tData))
        tLastWriteTime = tData.ftLastWriteTime;
    
    return tLastWriteTime;
}
#elif defined(__APPLE__)
struct timespec
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtimespec;
}
#else
static inline time_t
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtime;
}
#endif


bool
pl_has_library_changed(plSharedLibrary* ptLibrary)
{
    PL_ASSERT(ptLibrary);
    if(ptLibrary)
    {
        #ifdef _WIN32
        FILETIME newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        return CompareFileTime(&newWriteTime, &ptLibrary->tLastWriteTime) != 0;
        #elif defined(__APPLE__)
        struct timespec newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        return newWriteTime.tv_sec != ptLibrary->tLastWriteTime.tv_sec;
        #else
        time_t newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        return newWriteTime != ptLibrary->tLastWriteTime;
        #endif
    }
    return false;
}

plLibraryResult
pl_load_library(plLibraryDesc tDesc, plSharedLibrary** pptLibraryOut)
{

    plSharedLibrary* ptLibrary = NULL;

    if(*pptLibraryOut == NULL)
    {
        *pptLibraryOut = (plSharedLibrary*)PL_ALLOC(sizeof(plSharedLibrary));
        memset((*pptLibraryOut), 0, sizeof(plSharedLibrary));

        ptLibrary = *pptLibraryOut;

        ptLibrary->bValid = false;
        ptLibrary->tDesc = tDesc;

        #ifdef _WIN32
        pl_sprintf(ptLibrary->acPath, "%s.dll", tDesc.pcName);
        #elif defined(__APPLE__)
        pl_sprintf(ptLibrary->acPath, "%s.dylib", tDesc.pcName);
        #else
        pl_sprintf(ptLibrary->acPath, "./%s.so", tDesc.pcName);
        #endif

        if(tDesc.pcTransitionalName)
            strncpy(ptLibrary->acTransitionalName, tDesc.pcTransitionalName, PL_MAX_PATH_LENGTH);
        else
        {
            #ifdef _WIN32
            pl_sprintf(ptLibrary->acTransitionalName, "%s_", tDesc.pcName);
            #elif defined(__APPLE__)
            pl_sprintf(ptLibrary->acTransitionalName, "%s_", tDesc.pcName);
            #else
            pl_sprintf(ptLibrary->acTransitionalName, "./%s_", tDesc.pcName);
            #endif
        }

        if(tDesc.pcLockFile)
            strncpy(ptLibrary->acLockFile, tDesc.pcLockFile, PL_MAX_PATH_LENGTH);
        else
            strncpy(ptLibrary->acLockFile, "lock.tmp", PL_MAX_PATH_LENGTH);
    }
    else
        ptLibrary = *pptLibraryOut;

    ptLibrary->bValid = false;

    #ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA tIgnored;
    if(!GetFileAttributesExA(ptLibrary->acLockFile, GetFileExInfoStandard, &tIgnored))  // lock file gone
    #else
    struct stat attr2;
    if(stat(ptLibrary->acLockFile, &attr2) == -1)  // lock file gone
    #endif
    {
        char acTemporaryName[2024] = {0};
        ptLibrary->tLastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        
        #ifdef _WIN32
        pl_sprintf(acTemporaryName, "%s%u%s", ptLibrary->acTransitionalName, ptLibrary->uTempIndex, ".dll");
        #elif defined(__APPLE__)
        pl_sprintf(acTemporaryName, "%s%u%s", ptLibrary->acTransitionalName, ptLibrary->uTempIndex, ".dylib");
        #else
        pl_sprintf(acTemporaryName, "%s%u%s", ptLibrary->acTransitionalName, ptLibrary->uTempIndex, ".so");
        #endif
        if(++ptLibrary->uTempIndex >= 1024)
        {
            ptLibrary->uTempIndex = 0;
        }
        pl_copy_file(ptLibrary->acPath, acTemporaryName);

        
        #ifdef _WIN32
        ptLibrary->tHandle = NULL;
        ptLibrary->tHandle = LoadLibraryA(acTemporaryName);
        if(ptLibrary->tHandle)
            ptLibrary->bValid = true;
        else
        {
            DWORD iLastError = GetLastError();
            printf("LoadLibaryA() failed with error code : %d\n", iLastError);
        }
        #else
        ptLibrary->handle = NULL;
        ptLibrary->handle = dlopen(acTemporaryName, RTLD_NOW);
        if(ptLibrary->handle)
            ptLibrary->bValid = true;
        else
        {
            printf("\n\n%s\n\n", dlerror());
        }
        #endif
    }

    if(ptLibrary->bValid)
        return PL_LIBRARY_RESULT_SUCCESS;
    return PL_LIBRARY_RESULT_FAIL;
}

void
pl_reload_library(plSharedLibrary* ptLibrary)
{
    ptLibrary->bValid = false;
    for(uint32_t i = 0; i < 100; i++)
    {
        if(pl_load_library(ptLibrary->tDesc, &ptLibrary))
            break;
        // pl_sleep(100);
        #ifdef _WIN32
        Sleep((long)100);
        #else
        struct timespec ts = {0};
        int res;
    
        ts.tv_sec = 100 / 1000;
        ts.tv_nsec = (100 % 1000) * 1000000;
    
        do 
        {
            res = nanosleep(&ts, &ts);
        } 
        while (res);
        #endif
    }
}

void*
pl_load_library_function(plSharedLibrary* ptLibrary, const char* name)
{
    PL_ASSERT(ptLibrary->bValid && "library not valid, should have been checked");
    void* pLoadedFunction = NULL;
    if(ptLibrary->bValid)
    {
        #ifdef _WIN32
        pLoadedFunction = (void*)GetProcAddress(ptLibrary->tHandle, name);
        #else
        pLoadedFunction = dlsym(ptLibrary->handle, name);
        #endif
    }
    return pLoadedFunction;
}


void
pl_glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    // if(ImGui::GetIO().WantCaptureMouse)
    //     return;

    // update key modifiers
    gptIOI->add_key_event(PL_KEY_MOD_CTRL,  (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS));
    gptIOI->add_key_event(PL_KEY_MOD_SHIFT, (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)   == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT)   == GLFW_PRESS));
    gptIOI->add_key_event(PL_KEY_MOD_ALT,   (glfwGetKey(window, GLFW_KEY_LEFT_ALT)     == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_ALT)     == GLFW_PRESS));
    gptIOI->add_key_event(PL_KEY_MOD_SUPER, (glfwGetKey(window, GLFW_KEY_LEFT_SUPER)   == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_SUPER)   == GLFW_PRESS));

    plIO* ptIO = gptIOI->get_io();
    if (button >= 0 && button < PL_MOUSE_BUTTON_COUNT)
        gptIOI->add_mouse_button_event(button, action == GLFW_PRESS);
}

void
pl_glfw_mouse_pos_callback(GLFWwindow* window, double x, double y)
{
    gptIOI->add_mouse_pos_event((float)x, (float)y);
    tLastValidMousePos.x = (float)x;
    tLastValidMousePos.y = (float)y;
}

void
pl_glfw_cursor_enter_callback(GLFWwindow* window, int entered)
{
    // if(ImGui::GetIO().WantCaptureMouse)
    //     return;

    plIO* ptIO = gptIOI->get_io();
    if (entered)
    {
        ptMouseWindow = window;
        gptIOI->add_mouse_pos_event(tLastValidMousePos.x, tLastValidMousePos.y);
    }
    else if (ptMouseWindow == window)
    {
        tLastValidMousePos = ptIO->_tMousePos;
        ptMouseWindow = NULL;
        gptIOI->add_mouse_pos_event(-FLT_MAX, -FLT_MAX);
    }
}

void
pl_glfw_window_focus_callback(GLFWwindow* window, int focused)
{
    // Workaround for Linux: when losing focus with bMouseIgnoreButtonUpWaitForFocusLoss set, we will temporarily ignore subsequent Mouse Up events
    bMouseIgnoreButtonUp = (bMouseIgnoreButtonUpWaitForFocusLoss && focused == 0);
    bMouseIgnoreButtonUpWaitForFocusLoss = false;

    // gptIOI->add_focus_event(focused != 0);
}

void
pl_glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    // if(ImGui::GetIO().WantCaptureMouse)
    //     return;
    gptIOI->add_mouse_wheel_event((float)xoffset, (float)yoffset);
}

void
pl_glfw_char_callback(GLFWwindow* window, unsigned int c)
{
    // if(ImGui::GetIO().WantTextInput)
    //     return;
    gptIOI->add_text_event(c);
}

int
pl_translate_untranslated_key(int key, int scancode)
{
    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_EQUAL)
        return key;

        GLFWerrorfun prev_error_callback = glfwSetErrorCallback(NULL);
        const char* key_name = glfwGetKeyName(key, scancode);
        glfwSetErrorCallback(prev_error_callback);
        (void)glfwGetError(NULL);

        if (key_name && key_name[0] != 0 && key_name[1] == 0)
        {
            const char char_names[] = "`-=[]\\,;\'./";
            const int char_keys[] = { GLFW_KEY_GRAVE_ACCENT, GLFW_KEY_MINUS, GLFW_KEY_EQUAL, GLFW_KEY_LEFT_BRACKET, GLFW_KEY_RIGHT_BRACKET, GLFW_KEY_BACKSLASH, GLFW_KEY_COMMA, GLFW_KEY_SEMICOLON, GLFW_KEY_APOSTROPHE, GLFW_KEY_PERIOD, GLFW_KEY_SLASH, 0 };
            PL_ASSERT(PL_ARRAYSIZE(char_names) == PL_ARRAYSIZE(char_keys));
            if (key_name[0] >= '0' && key_name[0] <= '9')               { key = GLFW_KEY_0 + (key_name[0] - '0'); }
            else if (key_name[0] >= 'A' && key_name[0] <= 'Z')          { key = GLFW_KEY_A + (key_name[0] - 'A'); }
            else if (key_name[0] >= 'a' && key_name[0] <= 'z')          { key = GLFW_KEY_A + (key_name[0] - 'a'); }
            else
            {
                const char* p = strchr(char_names, key_name[0]);
                if(p)
                {
                    key = char_keys[p - char_names];
                }
            }

        }
        // if (action == GLFW_PRESS) printf("key %d scancode %d name '%s'\n", key, scancode, key_name);
    return key;
}

void
pl_glfw_size_callback(GLFWwindow* window, int width, int height)
{
    gptIOCtx->bViewportSizeChanged = true;
    if(width == 0 || height == 0)
    {
        gptIOCtx->bViewportMinimized = true;
    }
    else
    {
        int fwidth, fheight;
        glfwGetFramebufferSize(ptGlfwWindow, &fwidth, &fheight);
        gptIOCtx->tMainFramebufferScale.x = (float)fwidth / (float)width;
        gptIOCtx->tMainFramebufferScale.y = (float)fheight / (float)height;

        gptIOCtx->bViewportMinimized = false;
        gptIOCtx->tMainViewportSize.x = (float)width;
        gptIOCtx->tMainViewportSize.y = (float)height;
        // gsbtWindows[0]->tDesc.uWidth = (uint32_t)width;
        // gsbtWindows[0]->tDesc.uHeight = (uint32_t)height;

        pl_app_resize(gptMainWindow, gpUserData);
    }
}

void
pl_glfw_window_iconified_callback(GLFWwindow* window, int iconified)
{
    gptIOCtx->bViewportMinimized = iconified;
}

void
pl_glfw_window_close_callback(GLFWwindow* window)
{
    gptIOCtx->bRunning = false;
}

void
pl_glfw_key_callback(GLFWwindow* window, int keycode, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS && action != GLFW_RELEASE)
        return;

    // if(ImGui::GetIO().WantCaptureKeyboard)
    //     return;

    // update key modifiers
    gptIOI->add_key_event(PL_KEY_MOD_CTRL,  (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS));
    gptIOI->add_key_event(PL_KEY_MOD_SHIFT, (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)   == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT)   == GLFW_PRESS));
    gptIOI->add_key_event(PL_KEY_MOD_ALT,   (glfwGetKey(window, GLFW_KEY_LEFT_ALT)     == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_ALT)     == GLFW_PRESS));
    gptIOI->add_key_event(PL_KEY_MOD_SUPER, (glfwGetKey(window, GLFW_KEY_LEFT_SUPER)   == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_SUPER)   == GLFW_PRESS));

    if (keycode >= 0 && keycode < PL_ARRAYSIZE(atKeyOwnerWindows))
        atKeyOwnerWindows[keycode] = (action == GLFW_PRESS) ? window : NULL;

    keycode = pl_translate_untranslated_key(keycode, scancode);

    plKey imgui_key = pl_glfw_key_translate(keycode, scancode);
    gptIOI->add_key_event(imgui_key, (action == GLFW_PRESS));
}

plKey
pl_glfw_key_translate(int keycode, int scancode)
{
    switch (keycode)
    {
        case GLFW_KEY_TAB: return PL_KEY_TAB;
        case GLFW_KEY_LEFT: return PL_KEY_LEFT_ARROW;
        case GLFW_KEY_RIGHT: return PL_KEY_RIGHT_ARROW;
        case GLFW_KEY_UP: return PL_KEY_UP_ARROW;
        case GLFW_KEY_DOWN: return PL_KEY_DOWN_ARROW;
        case GLFW_KEY_PAGE_UP: return PL_KEY_PAGE_UP;
        case GLFW_KEY_PAGE_DOWN: return PL_KEY_PAGE_DOWN;
        case GLFW_KEY_HOME: return PL_KEY_HOME;
        case GLFW_KEY_END: return PL_KEY_END;
        case GLFW_KEY_INSERT: return PL_KEY_INSERT;
        case GLFW_KEY_DELETE: return PL_KEY_DELETE;
        case GLFW_KEY_BACKSPACE: return PL_KEY_BACKSPACE;
        case GLFW_KEY_SPACE: return PL_KEY_SPACE;
        case GLFW_KEY_ENTER: return PL_KEY_ENTER;
        case GLFW_KEY_ESCAPE: return PL_KEY_ESCAPE;
        case GLFW_KEY_APOSTROPHE: return PL_KEY_APOSTROPHE;
        case GLFW_KEY_COMMA: return PL_KEY_COMMA;
        case GLFW_KEY_MINUS: return PL_KEY_MINUS;
        case GLFW_KEY_PERIOD: return PL_KEY_PERIOD;
        case GLFW_KEY_SLASH: return PL_KEY_SLASH;
        case GLFW_KEY_SEMICOLON: return PL_KEY_SEMICOLON;
        case GLFW_KEY_EQUAL: return PL_KEY_EQUAL;
        case GLFW_KEY_LEFT_BRACKET: return PL_KEY_LEFT_BRACKET;
        case GLFW_KEY_BACKSLASH: return PL_KEY_BACKSLASH;
        // case GLFW_KEY_WORLD_1: return PL_KEY_Oem102;
        // case GLFW_KEY_WORLD_2: return PL_KEY_Oem102;
        case GLFW_KEY_RIGHT_BRACKET: return PL_KEY_RIGHT_BRACKET;
        case GLFW_KEY_GRAVE_ACCENT: return PL_KEY_GRAVE_ACCENT;
        case GLFW_KEY_CAPS_LOCK: return PL_KEY_CAPS_LOCK;
        case GLFW_KEY_SCROLL_LOCK: return PL_KEY_SCROLL_LOCK;
        case GLFW_KEY_NUM_LOCK: return PL_KEY_NUM_LOCK;
        case GLFW_KEY_PRINT_SCREEN: return PL_KEY_PRINT_SCREEN;
        case GLFW_KEY_PAUSE: return PL_KEY_PAUSE;
        case GLFW_KEY_KP_0: return PL_KEY_KEYPAD_0;
        case GLFW_KEY_KP_1: return PL_KEY_KEYPAD_1;
        case GLFW_KEY_KP_2: return PL_KEY_KEYPAD_2;
        case GLFW_KEY_KP_3: return PL_KEY_KEYPAD_3;
        case GLFW_KEY_KP_4: return PL_KEY_KEYPAD_4;
        case GLFW_KEY_KP_5: return PL_KEY_KEYPAD_5;
        case GLFW_KEY_KP_6: return PL_KEY_KEYPAD_6;
        case GLFW_KEY_KP_7: return PL_KEY_KEYPAD_7;
        case GLFW_KEY_KP_8: return PL_KEY_KEYPAD_8;
        case GLFW_KEY_KP_9: return PL_KEY_KEYPAD_9;
        case GLFW_KEY_KP_DECIMAL: return PL_KEY_KEYPAD_DECIMAL;
        case GLFW_KEY_KP_DIVIDE: return PL_KEY_KEYPAD_DIVIDE;
        case GLFW_KEY_KP_MULTIPLY: return PL_KEY_KEYPAD_MULTIPLY;
        case GLFW_KEY_KP_SUBTRACT: return PL_KEY_KEYPAD_SUBTRACT;
        case GLFW_KEY_KP_ADD: return PL_KEY_KEYPAD_ADD;
        case GLFW_KEY_KP_ENTER: return PL_KEY_KEYPAD_ENTER;
        case GLFW_KEY_KP_EQUAL: return PL_KEY_KEYPAD_EQUAL;
        case GLFW_KEY_LEFT_SHIFT: return PL_KEY_LEFT_SHIFT;
        case GLFW_KEY_LEFT_CONTROL: return PL_KEY_LEFT_CTRL;
        case GLFW_KEY_LEFT_ALT: return PL_KEY_LEFT_ALT;
        case GLFW_KEY_LEFT_SUPER: return PL_KEY_LEFT_SUPER;
        case GLFW_KEY_RIGHT_SHIFT: return PL_KEY_RIGHT_SHIFT;
        case GLFW_KEY_RIGHT_CONTROL: return PL_KEY_RIGHT_CTRL;
        case GLFW_KEY_RIGHT_ALT: return PL_KEY_RIGHT_ALT;
        case GLFW_KEY_RIGHT_SUPER: return PL_KEY_RIGHT_SUPER;
        case GLFW_KEY_MENU: return PL_KEY_MENU;
        case GLFW_KEY_0: return PL_KEY_0;
        case GLFW_KEY_1: return PL_KEY_1;
        case GLFW_KEY_2: return PL_KEY_2;
        case GLFW_KEY_3: return PL_KEY_3;
        case GLFW_KEY_4: return PL_KEY_4;
        case GLFW_KEY_5: return PL_KEY_5;
        case GLFW_KEY_6: return PL_KEY_6;
        case GLFW_KEY_7: return PL_KEY_7;
        case GLFW_KEY_8: return PL_KEY_8;
        case GLFW_KEY_9: return PL_KEY_9;
        case GLFW_KEY_A: return PL_KEY_A;
        case GLFW_KEY_B: return PL_KEY_B;
        case GLFW_KEY_C: return PL_KEY_C;
        case GLFW_KEY_D: return PL_KEY_D;
        case GLFW_KEY_E: return PL_KEY_E;
        case GLFW_KEY_F: return PL_KEY_F;
        case GLFW_KEY_G: return PL_KEY_G;
        case GLFW_KEY_H: return PL_KEY_H;
        case GLFW_KEY_I: return PL_KEY_I;
        case GLFW_KEY_J: return PL_KEY_J;
        case GLFW_KEY_K: return PL_KEY_K;
        case GLFW_KEY_L: return PL_KEY_L;
        case GLFW_KEY_M: return PL_KEY_M;
        case GLFW_KEY_N: return PL_KEY_N;
        case GLFW_KEY_O: return PL_KEY_O;
        case GLFW_KEY_P: return PL_KEY_P;
        case GLFW_KEY_Q: return PL_KEY_Q;
        case GLFW_KEY_R: return PL_KEY_R;
        case GLFW_KEY_S: return PL_KEY_S;
        case GLFW_KEY_T: return PL_KEY_T;
        case GLFW_KEY_U: return PL_KEY_U;
        case GLFW_KEY_V: return PL_KEY_V;
        case GLFW_KEY_W: return PL_KEY_W;
        case GLFW_KEY_X: return PL_KEY_X;
        case GLFW_KEY_Y: return PL_KEY_Y;
        case GLFW_KEY_Z: return PL_KEY_Z;
        case GLFW_KEY_F1: return PL_KEY_F1;
        case GLFW_KEY_F2: return PL_KEY_F2;
        case GLFW_KEY_F3: return PL_KEY_F3;
        case GLFW_KEY_F4: return PL_KEY_F4;
        case GLFW_KEY_F5: return PL_KEY_F5;
        case GLFW_KEY_F6: return PL_KEY_F6;
        case GLFW_KEY_F7: return PL_KEY_F7;
        case GLFW_KEY_F8: return PL_KEY_F8;
        case GLFW_KEY_F9: return PL_KEY_F9;
        case GLFW_KEY_F10: return PL_KEY_F10;
        case GLFW_KEY_F11: return PL_KEY_F11;
        case GLFW_KEY_F12: return PL_KEY_F12;
        case GLFW_KEY_F13: return PL_KEY_F13;
        case GLFW_KEY_F14: return PL_KEY_F14;
        case GLFW_KEY_F15: return PL_KEY_F15;
        case GLFW_KEY_F16: return PL_KEY_F16;
        case GLFW_KEY_F17: return PL_KEY_F17;
        case GLFW_KEY_F18: return PL_KEY_F18;
        case GLFW_KEY_F19: return PL_KEY_F19;
        case GLFW_KEY_F20: return PL_KEY_F20;
        case GLFW_KEY_F21: return PL_KEY_F21;
        case GLFW_KEY_F22: return PL_KEY_F22;
        case GLFW_KEY_F23: return PL_KEY_F23;
        case GLFW_KEY_F24: return PL_KEY_F24;
        default: return PL_KEY_NONE;
    }
}

void
pl_glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl.c"
#include "imgui_impl_glfw.cpp"