###############################################################################
#                              file index                                     #
###############################################################################
#                               imports                                       #
#                               project                                       #
#                            pilotlight_lib                                   #
#                              extensions                                     #
#                               scripts                                       #
#                                 app                                         #
#                             pilot_light                                     #
#                           generate scripts                                  #
###############################################################################

###############################################################################
#                               imports                                       #
###############################################################################

import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/../pl_build")

import pl_build as pl
import pl_build_win32 as win32
import pl_build_linux as linux
import pl_build_macos as apple

###############################################################################
#                               project                                       #
###############################################################################

with pl.project("pilotlight"):
    
    # configurations
    pl.add_configuration("debug")
    pl.add_configuration("vulkan") # only used on macos for vulkan

    # where to output build scripts
    pl.set_working_directory(os.path.dirname(os.path.abspath(__file__)) + "/../src")

    # used to decide hot reloading
    pl.set_hot_reload_target("../out/pilot_light")

    # project wide settings
    pl.set_output_directory("../out")
    pl.add_link_directories("../out")
    pl.add_definitions("_USE_MATH_DEFINES", "PL_PROFILING_ON", "PL_ALLOW_HOT_RELOAD", "PL_ENABLE_VALIDATION_LAYERS", "_DEBUG")
    pl.add_include_directories("../sandbox", "../src", "../libs", "../extensions", "../out", "../dependencies/stb", "../dependencies/cgltf")

    # profiles - backend defines
    pl.add_definitions_profile(None, ["vulkan"], None, None, "PL_VULKAN_BACKEND")
    pl.add_definitions_profile(None, ["debug"], None, ["gcc", "msvc"], "PL_VULKAN_BACKEND")
    pl.add_definitions_profile(None, ["debug"], ["Darwin"], None, "PL_METAL_BACKEND")

    # profiles - directories
    pl.add_include_directories_profile(None, None, ["Windows"], None,'%WindowsSdkDir%Include\\um', '%WindowsSdkDir%Include\\shared')
    pl.add_link_directories_profile(None, None, ["Linux"], None, "/usr/lib/x86_64-linux-gnu")
    pl.add_link_frameworks_profile(None, None, ["Darwin"], None, "Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

    # profiles - flags
    pl.add_linker_flags_profile(None, None, ["Darwin"], None, "-Wl,-rpath,/usr/local/lib")
    pl.add_linker_flags_profile(None, None, ["Linux"], None, "-ldl", "-lm")
    pl.add_compiler_flags_profile(None, None, None, ["gcc"], "-std=gnu11", "-fPIC", "--debug", "-g")
    pl.add_compiler_flags_profile(None, None, None, ["clang"], "-std=c99", "--debug", "-g", "-fmodules", "-ObjC", "-fPIC")
    pl.add_compiler_flags_profile(None, None, None, ["msvc"], "-Zc:preprocessor", "-nologo", "-std:c11", "-W4", "-WX", "-wd4201",
                              "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                              "-permissive-", "-Od", "-MDd", "-Zi")

    ###############################################################################
    #                            pilotlight_lib                                   #
    ###############################################################################

    with pl.target("pilotlight_lib", pl.TargetType.STATIC_LIBRARY):

        pl.add_source_files("pilotlight_lib.c")

        # default config
        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.set_output_binary("pilotlight")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.set_output_binary("pilotlight")
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread")
   
            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.set_output_binary("pilotlight")
                    
        # vulkan on macos
        with pl.configuration("vulkan"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.set_output_binary("pilotlight")
                    
    ###############################################################################
    #                                 extensions                                  #
    ###############################################################################

    # vulkan backend extensions
    with pl.target("pl_extensions", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.add_static_link_libraries("pilotlight")
        pl.add_source_files("pl_extensions.c")
        pl.set_output_binary("pl_extensions")

        # default config
        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("shaderc_combined")
  
                    # vulkan stuff
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_static_link_libraries("vulkan-1")
                    pl.add_linker_flags("-nodefaultlib:MSVCRT")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("shaderc_shared", "xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread")

                    # vulkan stuff
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
    with pl.target("pl_extensions", pl.TargetType.DYNAMIC_LIBRARY, True):

        # default config
        with pl.configuration("debug"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_static_link_libraries("pilotlight")
                    pl.add_source_files("pl_extensions.c")
                    pl.set_output_binary("pl_extensions")
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared")
                    

    ###############################################################################
    #                                 scripts                                     #
    ###############################################################################

    # vulkan backend
    with pl.target("pl_script_camera", pl.TargetType.DYNAMIC_LIBRARY, True):


        pl.add_static_link_libraries("pilotlight")
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

    ###############################################################################
    #                                   app                                       #
    ###############################################################################

    with pl.target("app", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.add_static_link_libraries("pilotlight")
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
                    pl.set_output_binary("app")

        # vulkan on macos
        with pl.configuration("vulkan"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

    ###############################################################################
    #                                 pilot_light                                 #
    ###############################################################################

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

###############################################################################
#                           generate scripts                                  #
###############################################################################

win32.generate_build("build_win32.bat", "Windows", "msvc", {"dev env setup" : True})
linux.generate_build("build_linux.sh", "Linux", "gcc", None)
apple.generate_build("build_macos.sh", "Darwin", "clang", None)