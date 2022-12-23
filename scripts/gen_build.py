import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/../tools")

import pl_build as pl

pl.register_standard_profiles()

with pl.project("pilotlight"):
    
    # configurations
    pl.add_configuration("debug")
    pl.add_configuration("debugdx11")   # only used on win32 for direct x11
    pl.add_configuration("debugmetal") # only used on macos for vulkan

    # where to output build scripts
    pl.set_working_directory(os.path.dirname(os.path.abspath(__file__)) + "/../src")

    # used to decide hot reloading
    pl.set_main_target("pilot_light")

    pl.push_profile(pl.Profile.PILOT_LIGHT_DEBUG)

    pl.push_definitions("_USE_MATH_DEFINES", "PL_PROFILING_ON", "PL_ALLOW_HOT_RELOAD", "PL_ENABLE_VALIDATION_LAYERS")
    pl.push_include_directories("../out", "../dependencies/stb", "../src", "../extensions")
    pl.push_link_directories("../out")
    pl.push_output_directory("../out")
        
    ###############################################################################
    #                                 pl_lib                                      #
    ###############################################################################
    with pl.target("pl_lib", pl.TargetType.STATIC_LIBRARY):

        pl.push_source_files("pilotlight.c")
        pl.push_output_binary("pilotlight")

        pl.push_profile(pl.Profile.VULKAN)
        pl.push_definitions("PL_VULKAN_BACKEND")
        with pl.configuration("debug"):
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pass
            with pl.platform(pl.PlatformType.LINUX):
                with pl.compiler("gcc", pl.CompilerType.GCC):
                    pass
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pass
        pl.pop_definitions()
        pl.pop_profile() 

        with pl.configuration("debugdx11"):
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pl.add_definition("PL_DX11_BACKEND")

        with pl.configuration("debugmetal"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")

        pl.pop_output_binary()
        pl.pop_source_files()

    ###############################################################################
    #                                 pl_draw_extension                           #
    ###############################################################################
    with pl.target("pl_draw_extension", pl.TargetType.DYNAMIC_LIBRARY):
        
        pl.push_output_binary("pl_draw_extension")
        pl.push_source_files("../extensions/pl_draw_extension.c")
        pl.push_target_links("pl_lib")

        pl.push_profile(pl.Profile.VULKAN)
        pl.push_definitions("PL_VULKAN_BACKEND")
        with pl.configuration("debug"):
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pass
            with pl.platform(pl.PlatformType.LINUX):
                with pl.compiler("gcc", pl.CompilerType.GCC):
                    pass
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pass
        pl.pop_definitions()
        pl.pop_profile() 

        with pl.configuration("debugdx11"):
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pl.add_definition("PL_DX11_BACKEND")

        with pl.configuration("debugmetal"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")

        pl.pop_output_binary()
        pl.pop_source_files()
        pl.pop_target_links()

    ###############################################################################
    #                                    app                                      #
    ###############################################################################
    with pl.target("app", pl.TargetType.DYNAMIC_LIBRARY):

        pl.push_output_binary("app")
        pl.push_target_links("pl_lib")

        pl.push_profile(pl.Profile.VULKAN)
        pl.push_definitions("PL_VULKAN_BACKEND")
        pl.push_source_files("app_vulkan.c")
        with pl.configuration("debug"):
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pass
            with pl.platform(pl.PlatformType.LINUX):
                with pl.compiler("gcc", pl.CompilerType.GCC):
                    pass
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pass
        pl.pop_definitions()
        pl.pop_source_files()
        pl.pop_profile() 

        with pl.configuration("debugdx11"):
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pl.add_source_file("app_dx11.c")
                    pl.add_definition("PL_DX11_BACKEND")

        with pl.configuration("debugmetal"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")
                    pl.add_source_file("app_metal.m")

        pl.pop_output_binary()
        pl.pop_target_links()

    ###############################################################################
    #                                 pilot_light                                 #
    ###############################################################################
    with pl.target("pilot_light", pl.TargetType.EXECUTABLE):

        pl.push_output_binary("pilot_light")
        pl.push_target_links("pl_lib")
               
        pl.push_profile(pl.Profile.VULKAN)
        pl.push_definitions("PL_VULKAN_BACKEND")
        with pl.configuration("debug"):
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pl.add_source_file("pl_main_win32.c")
                    pl.add_definition("_DEBUG")          
            with pl.platform(pl.PlatformType.LINUX):
                with pl.compiler("gcc", pl.CompilerType.GCC):
                    pl.add_source_file("pl_main_linux.c")
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_source_file("pl_main_macos.m")
        pl.pop_definitions()
        pl.pop_profile() 

        with pl.configuration("debugdx11"):
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pl.add_source_file("pl_main_win32.c")
                    pl.add_definition("PL_DX11_BACKEND")

        with pl.configuration("debugmetal"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")
                    pl.add_source_file("pl_main_macos.m")

        pl.pop_output_binary()
        pl.pop_target_links()

    pl.pop_definitions()
    pl.pop_include_directories()
    pl.pop_link_directories()
    pl.pop_output_directory()  
    pl.pop_profile()    
pl.generate_build_script("build")