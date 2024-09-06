# gen_core.py

# Index of this file:
# [SECTION] imports
# [SECTION] project
# [SECTION] profiles
# [SECTION] pl_lib
# [SECTION] extensions
# [SECTION] scripts
# [SECTION] app
# [SECTION] pilot_light
# [SECTION] generate_scripts

#-----------------------------------------------------------------------------
# [SECTION] imports
#-----------------------------------------------------------------------------

import os
import sys

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
    pl.add_definitions("_USE_MATH_DEFINES", "PL_PROFILING_ON", "PL_ALLOW_HOT_RELOAD", "PL_ENABLE_VALIDATION_LAYERS", "_DEBUG")
    pl.add_include_directories("../sandbox", "../src", "../libs", "../extensions", "../out", "../dependencies/stb", "../dependencies/cgltf")

    # profiles - backend defines
    pl.add_definitions_profile("PL_VULKAN_BACKEND", configurations=["vulkan"])
    pl.add_definitions_profile("PL_VULKAN_BACKEND", configurations=["debug"], compilers=["gcc", "msvc"])
    pl.add_definitions_profile("PL_METAL_BACKEND", configurations=["debug"], platforms=["Darwin"])

    # profiles - directories
    pl.add_include_directories_profile('%WindowsSdkDir%Include\\um', '%WindowsSdkDir%Include\\shared', platforms=["Windows"])
    pl.add_link_directories_profile("/usr/lib/x86_64-linux-gnu", platforms=["Linux"])
    pl.add_link_frameworks_profile("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore", platforms=["Darwin"])

    # profiles - flags
    pl.add_linker_flags_profile("-Wl,-rpath,/usr/local/lib", platforms=["Darwin"])
    pl.add_linker_flags_profile("-ldl", "-lm", platforms=["Linux"])
    pl.add_compiler_flags_profile("-std=gnu11", "-fPIC", "--debug", "-g", compilers=["gcc"])
    pl.add_compiler_flags_profile("-std=c99", "--debug", "-g", "-fmodules", "-ObjC", "-fPIC", compilers=["clang"])
    pl.add_compiler_flags_profile("-Zc:preprocessor", "-nologo", "-std:c11", "-W4", "-WX", "-wd4201",
                              "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                              "-permissive-", "-Od", "-MDd", "-Zi", compilers=["msvc"])

    #-----------------------------------------------------------------------------
    # [SECTION] pl_lib
    #-----------------------------------------------------------------------------

    with pl.target("pl_lib", pl.TargetType.STATIC_LIBRARY):

        pl.add_source_files("pl_lib.c")
        pl.set_output_binary("pilot_light")

        # default config
        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pass

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread")
   
            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass
                    
        # vulkan on macos
        with pl.configuration("vulkan"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass
                    
    #-----------------------------------------------------------------------------
    # [SECTION] extensions
    #-----------------------------------------------------------------------------

    # vulkan backend extensions
    with pl.target("pl_ext", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.add_static_link_libraries("pilot_light")
        pl.add_source_files("pl_ext.c")
        pl.set_output_binary("pilot_light")

        # default config
        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("shaderc_combined")
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_static_link_libraries("vulkan-1")
                    pl.add_linker_flags("-nodefaultlib:MSVCRT")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("shaderc_shared", "xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_dynamic_link_libraries("vulkan")

        # vulkan on macos
        with pl.configuration("vulkan"):

            # mac os
            with pl.platform("Darwin"):

                with pl.compiler("clang"):
                    pl.add_dynamic_link_libraries("shaderc_shared", "pthread")
                    pl.add_dynamic_link_libraries("vulkan")

    # metal backend extensions
    with pl.target("pl_ext", pl.TargetType.DYNAMIC_LIBRARY, True):

        # default config
        with pl.configuration("debug"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_static_link_libraries("pilot_light")
                    pl.add_source_files("pl_ext.c")
                    pl.set_output_binary("pilot_light")
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared")
                    
    #-----------------------------------------------------------------------------
    # [SECTION] scripts
    #-----------------------------------------------------------------------------

    # vulkan backend
    with pl.target("pl_script_camera", pl.TargetType.DYNAMIC_LIBRARY, True):


        pl.add_static_link_libraries("pilot_light")
        pl.add_source_files("../extensions/pl_script_camera.c")
        pl.set_output_binary("pl_script_camera")

        # default config
        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pass
                              
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pass

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

        # vulkan on macos
        with pl.configuration("vulkan"):

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_dynamic_link_libraries("shaderc_shared")

    #-----------------------------------------------------------------------------
    # [SECTION] app
    #-----------------------------------------------------------------------------

    with pl.target("app", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.add_static_link_libraries("pilot_light")
        pl.add_source_files("../sandbox/app.c")
        pl.set_output_binary("app")

        # default config
        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pass
            
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

        # vulkan on macos
        with pl.configuration("vulkan"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

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
                    pl.add_source_files("pl_main_win32.c")
                    
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("pl_main_x11.c")
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("pl_main_macos.m")
        
        # vulkan on macos
        with pl.configuration("vulkan"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("pl_main_macos.m")

#-----------------------------------------------------------------------------
# [SECTION] generate scripts
#-----------------------------------------------------------------------------

win32.generate_build(working_directory + '/' + "build_win32.bat", {"dev env setup" : True})
linux.generate_build(working_directory + '/' + "build_linux.sh")
apple.generate_build(working_directory + '/' + "build_macos.sh")