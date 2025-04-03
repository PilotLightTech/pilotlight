# gen_core.py

# Index of this file:
# [SECTION] imports
# [SECTION] project
# [SECTION] profiles
# [SECTION] pl_lib
# [SECTION] extensions
# [SECTION] experimental extensions
# [SECTION] scripts
# [SECTION] app
# [SECTION] pilot_light
# [SECTION] generate_scripts

#-----------------------------------------------------------------------------
# [SECTION] imports
#-----------------------------------------------------------------------------

import os
import sys
import platform as plat

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/..")

import pl_build.core as pl
import pl_build.backend_win32 as win32
import pl_build.backend_linux as linux
import pl_build.backend_macos as apple

#-----------------------------------------------------------------------------
# [SECTION] project
#-----------------------------------------------------------------------------

# where to output build scripts
working_directory = os.path.dirname(os.path.abspath(__file__)) + "/../src"

with pl.project("pilotlight"):
    
    # used to decide hot reloading
    pl.set_hot_reload_target("../out/pilot_light")

    # project wide settings
    pl.set_output_directory("../out")
    pl.add_link_directories("../out")
    pl.add_include_directories('../dependencies/glfw/include', "../sandbox", "../src", "../libs", "../extensions",
                               "../out", "../dependencies/stb", "../dependencies/cgltf", "../dependencies/imgui",
                               "../dependencies/imgui/backends")
              
    #-----------------------------------------------------------------------------
    # [SECTION] glfw
    #-----------------------------------------------------------------------------

    with pl.target("glfw", pl.TargetType.STATIC_LIBRARY, False):

        pl.add_source_files("../dependencies/glfw/src/context.c")
        pl.add_source_files("../dependencies/glfw/src/init.c")
        pl.add_source_files("../dependencies/glfw/src/input.c")
        pl.add_source_files("../dependencies/glfw/src/monitor.c")
        pl.add_source_files("../dependencies/glfw/src/window.c")
        pl.add_source_files("../dependencies/glfw/src/platform.c")

        pl.add_source_files("../dependencies/glfw/src/osmesa_context.c")
        pl.add_source_files("../dependencies/glfw/src/egl_context.c")
        pl.add_source_files("../dependencies/glfw/src/vulkan.c")

        pl.add_source_files("../dependencies/glfw/src/null_init.c")
        pl.add_source_files("../dependencies/glfw/src/null_monitor.c")
        pl.add_source_files("../dependencies/glfw/src/null_joystick.c")
        pl.add_source_files("../dependencies/glfw/src/null_window.c")

        with pl.configuration("debug"):

            pl.set_output_binary("glfwd")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_definitions("UNICODE", "_UNICODE", "_CRT_SECURE_NO_WARNINGS", "_GLFW_VULKAN_STATIC", "_GLFW_WIN32", "_DEBUG")

                    pl.add_compiler_flags("-nologo", "-std:c11", "-W3", "-wd5105",
                                          "-Od", "-MDd", "-Zi", "-permissive")
                    
                    pl.add_linker_flags("-incremental:no", "-nologo")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    

                    pl.add_source_files("../dependencies/glfw/src/wgl_context.c")
                    pl.add_source_files("../dependencies/glfw/src/win32_time.c")
                    pl.add_source_files("../dependencies/glfw/src/win32_thread.c")
                    pl.add_source_files("../dependencies/glfw/src/win32_init.c")
                    pl.add_source_files("../dependencies/glfw/src/win32_joystick.c")
                    pl.add_source_files("../dependencies/glfw/src/win32_monitor.c")
                    pl.add_source_files("../dependencies/glfw/src/win32_window.c")
                    pl.add_source_files("../dependencies/glfw/src/win32_module.c")
                    pl.add_source_files("../dependencies/glfw/src/wgl_context.c")
            
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_definitions("_GLFW_VULKAN_STATIC", "_GLFW_X11", "_DEBUG")

                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan', '/usr/include/vulkan')
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "pthread", "xcb-cursor")
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_dynamic_link_libraries("vulkan")

                    pl.add_compiler_flags("-std=gnu99", "--debug -g")
                    pl.add_linker_flags("-ldl -lm")

                    pl.add_source_files("../dependencies/glfw/src/glx_context.c")

                    pl.add_source_files("../dependencies/glfw/src/posix_module.c")
                    pl.add_source_files("../dependencies/glfw/src/posix_poll.c")
                    pl.add_source_files("../dependencies/glfw/src/posix_thread.c")
                    pl.add_source_files("../dependencies/glfw/src/posix_time.c")
                    pl.add_source_files("../dependencies/glfw/src/x11_init.c")
                    pl.add_source_files("../dependencies/glfw/src/x11_monitor.c")
                    pl.add_source_files("../dependencies/glfw/src/x11_window.c")
                    pl.add_source_files("../dependencies/glfw/src/xkb_unicode.c")
                    pl.add_source_files("../dependencies/glfw/src/linux_joystick.c")

            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("_GLFW_VULKAN_STATIC", "_GLFW_COCOA", "_DEBUG")

                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan', '/usr/include/vulkan')
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared")
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_dynamic_link_libraries("vulkan")

                    pl.add_compiler_flags("-Wno-deprecated-declarations", "--debug -g")
                    pl.add_compiler_flags("-std=c99", "-fmodules", "-ObjC", "-fPIC")
                    pl.add_linker_flags("-Wl,-rpath,/usr/local/lib")
                    pl.add_link_frameworks("Cocoa", "IOKit", "CoreFoundation")

                    pl.add_source_files("../dependencies/glfw/src/posix_module.c")
                    # pl.add_source_files("../dependencies/glfw/src/posix_poll.c")
                    pl.add_source_files("../dependencies/glfw/src/posix_thread.c")
                    pl.add_source_files("../dependencies/glfw/src/cocoa_time.c")
                    pl.add_source_files("../dependencies/glfw/src/cocoa_init.m")
                    pl.add_source_files("../dependencies/glfw/src/cocoa_joystick.m")
                    pl.add_source_files("../dependencies/glfw/src/cocoa_monitor.m")
                    pl.add_source_files("../dependencies/glfw/src/cocoa_window.m")
                    pl.add_source_files("../dependencies/glfw/src/nsgl_context.m")

        with pl.configuration("vulkan"):

            pl.set_output_binary("glfwd")

            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("_GLFW_VULKAN_STATIC", "_GLFW_COCOA", "_DEBUG")

                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan', '/usr/include/vulkan')
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared")
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_dynamic_link_libraries("vulkan")

                    pl.add_compiler_flags("-Wno-deprecated-declarations", "--debug -g")
                    pl.add_compiler_flags("-std=c99", "-fmodules", "-ObjC", "-fPIC")
                    pl.add_linker_flags("-Wl,-rpath,/usr/local/lib")
                    pl.add_link_frameworks("Cocoa", "IOKit", "CoreFoundation")

                    pl.add_source_files("../dependencies/glfw/src/posix_module.c")
                    # pl.add_source_files("../dependencies/glfw/src/posix_poll.c")
                    pl.add_source_files("../dependencies/glfw/src/posix_thread.c")
                    pl.add_source_files("../dependencies/glfw/src/cocoa_time.c")
                    pl.add_source_files("../dependencies/glfw/src/cocoa_init.m")
                    pl.add_source_files("../dependencies/glfw/src/cocoa_joystick.m")
                    pl.add_source_files("../dependencies/glfw/src/cocoa_monitor.m")
                    pl.add_source_files("../dependencies/glfw/src/cocoa_window.m")
                    pl.add_source_files("../dependencies/glfw/src/nsgl_context.m")

    #-----------------------------------------------------------------------------
    # [SECTION] app
    #-----------------------------------------------------------------------------

    with pl.target("app", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.add_source_files("../sandbox/app.cpp")
        pl.set_output_binary("app")

        # default config
        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                pl.add_definitions("PL_VULKAN_BACKEND")
                with pl.compiler("msvc"):
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_source_files("../dependencies/imgui/imgui_unity.cpp")
                    pl.add_source_files("../dependencies/imgui/backends/imgui_impl_vulkan.cpp")

                    pl.add_linker_flags("-incremental:no", "-nologo")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_static_link_libraries("glfwd", "ucrtd")
                    pl.add_static_link_libraries("vulkan-1")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-W3", "-WX", "-wd4201", "-wd4100",
                                          "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                          "-Od", "-MDd", "-Zi", "-permissive")

            # linux
            with pl.platform("Linux"):
                pl.add_definitions("PL_VULKAN_BACKEND")
                with pl.compiler("gcc"):

                    pl.add_compiler_flags("-fPIC")
                    pl.add_linker_flags("-lstdc++")
                    pl.add_linker_flags("-ldl -lm")

                    pl.add_source_files("../dependencies/imgui/imgui_unity.cpp")
                    pl.add_source_files("../dependencies/imgui/backends/imgui_impl_vulkan.cpp")

                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_dynamic_link_libraries("vulkan")

            # apple
            with pl.platform("Darwin"):
                pl.add_definitions("PL_METAL_BACKEND")
                with pl.compiler("clang"):
                    pl.add_linker_flags("-lstdc++")
                    pl.add_linker_flags("-ldl -lm")
                    pl.add_compiler_flags("-fPIC", "-ObjC++", "--debug", "-g", "-std=c++14")

                    pl.add_source_files("../dependencies/imgui/imgui_unity.cpp")
                    pl.add_source_files("../dependencies/imgui/backends/imgui_impl_metal.mm")

                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")
                    # pl.add_link_frameworks("Cocoa", "IOKit", "CoreFoundation")

        with pl.configuration("vulkan"):

            pl.add_definitions("PL_VULKAN_BACKEND")

            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_linker_flags("-lstdc++")
                    pl.add_linker_flags("-ldl -lm", "-Wl,-rpath,/usr/local/lib")
                    pl.add_compiler_flags("-fPIC", "-fmodules", "--debug", "-g", "-std=c++14")

                    pl.add_source_files("../dependencies/imgui/imgui_unity.cpp")
                    pl.add_source_files("../dependencies/imgui/backends/imgui_impl_vulkan.cpp")

                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared", "pthread")
                    pl.add_dynamic_link_libraries("vulkan")
            

    #-----------------------------------------------------------------------------
    # [SECTION] pilot_light
    #-----------------------------------------------------------------------------

    with pl.target("pilot_light", pl.TargetType.EXECUTABLE):
    
        pl.set_output_binary("pilot_light")

        # default config
        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):

                    pl.add_definitions("PL_VULKAN_BACKEND")

                    pl.add_source_files("pl_main_glfw.cpp")
                    pl.add_source_files("../dependencies/imgui/imgui_unity.cpp")
                    pl.add_source_files("../dependencies/imgui/backends/imgui_impl_glfw.cpp")

                    pl.add_linker_flags("-incremental:no", "-nologo")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_static_link_libraries("user32", "Shell32", "Ole32", "gdi32")
                    pl.add_static_link_libraries("glfwd", "ucrtd")
                    pl.add_static_link_libraries("vulkan-1")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-W3", "-WX", "-wd4201", "-wd4100",
                                          "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                          "-Od", "-MDd", "-Zi", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):

                    pl.add_definitions("PL_VULKAN_BACKEND")

                    pl.add_source_files("pl_main_glfw.cpp")
                    pl.add_compiler_flags("-fPIC")
                    pl.add_compiler_flags("--debug -g")
                    pl.add_linker_flags("-lstdc++")
                    pl.add_linker_flags("-ldl -lm")

                    pl.add_source_files("../dependencies/imgui/imgui_unity.cpp")
                    pl.add_source_files("../dependencies/imgui/backends/imgui_impl_glfw.cpp")

                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_dynamic_link_libraries("vulkan")
                    pl.add_static_link_libraries("glfwd")

            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):

                    pl.add_definitions("PL_METAL_BACKEND")

                    pl.add_source_files("pl_main_glfw.cpp")
                    pl.add_compiler_flags("-fPIC", "-ObjC++")
                    pl.add_compiler_flags("--debug -g", "-std=c++11")
                    pl.add_linker_flags("-lstdc++")
                    pl.add_linker_flags("-ldl -lm")

                    pl.add_source_files("../dependencies/imgui/imgui_unity.cpp")
                    pl.add_source_files("../dependencies/imgui/backends/imgui_impl_glfw.cpp")

                    pl.add_static_link_libraries("glfwd")

        with pl.configuration("vulkan"):

            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):

                    pl.add_definitions("PL_VULKAN_BACKEND")

                    pl.add_source_files("pl_main_glfw.cpp")
                    pl.add_compiler_flags("-fPIC", "-ObjC++", "-fmodules")
                    pl.add_compiler_flags("--debug -g", "-std=c++11")
                    pl.add_linker_flags("-lstdc++")
                    pl.add_linker_flags("-ldl -lm")
                    pl.add_linker_flags("-Wl,-rpath,/usr/local/lib")

                    pl.add_source_files("../dependencies/imgui/imgui_unity.cpp")
                    pl.add_source_files("../dependencies/imgui/backends/imgui_impl_glfw.cpp")
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared", "pthread")
                    pl.add_dynamic_link_libraries("vulkan")
                    pl.add_static_link_libraries("glfwd")
                    
#-----------------------------------------------------------------------------
# [SECTION] generate scripts
#-----------------------------------------------------------------------------

if plat.system() == "Windows":
    win32.generate_build(working_directory + '/' + "build_glfw.bat")
elif plat.system() == "Darwin":
    apple.generate_build(working_directory + '/' + "build_glfw.sh")
elif plat.system() == "Linux":
    linux.generate_build(working_directory + '/' + "build_glfw.sh")