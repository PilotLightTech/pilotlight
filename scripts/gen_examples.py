import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/../pl_build")

import pl_build as pl

###############################################################################
#                                helpers                                      #
###############################################################################

def add_example_app(name):

    with pl.target(name, pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.push_output_binary(name)
        pl.push_target_links("pilotlight_lib")
        pl.push_source_files(name + ".c")
        
        with pl.configuration("debug"):
            pl.push_profile(pl.Profile.VULKAN)
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pl.add_definition("PL_VULKAN_BACKEND")
            with pl.platform(pl.PlatformType.LINUX):
                with pl.compiler("gcc", pl.CompilerType.GCC):
                    pl.add_definition("PL_VULKAN_BACKEND")
            pl.pop_profile() 
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")
        
        
        pl.pop_source_files()
        pl.pop_output_binary()
        pl.pop_target_links()

pl.register_standard_profiles()

with pl.project("pilotlight"):
    
    # configurations
    pl.add_configuration("debug")

    # where to output build scripts
    pl.set_working_directory(os.path.dirname(os.path.abspath(__file__)) + "/../examples")

    # used to decide hot reloading
    pl.set_main_target("pilot_light")

    pl.push_profile(pl.Profile.PILOT_LIGHT_DEBUG_C)

    pl.push_definitions("_USE_MATH_DEFINES", "PL_PROFILING_ON", "PL_ALLOW_HOT_RELOAD", "PL_ENABLE_VALIDATION_LAYERS")
    pl.push_include_directories("../apps", "../examples", "../src", "../libs", "../extensions", "../out", "../dependencies/stb", "../dependencies/cgltf")
    pl.push_link_directories("../out")
    pl.push_output_directory("../out")
        
    ###############################################################################
    #                            pilotlight_lib                                   #
    ###############################################################################
    with pl.target("pilotlight_lib", pl.TargetType.STATIC_LIBRARY):

        pl.push_source_files("../src/pilotlight_lib.c")
        pl.push_output_binary("pilotlight")

        with pl.configuration("debug"):
            pl.push_profile(pl.Profile.VULKAN)
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pl.add_definition("PL_VULKAN_BACKEND")
            with pl.platform(pl.PlatformType.LINUX):
                with pl.compiler("gcc", pl.CompilerType.GCC):
                    pl.add_definition("PL_VULKAN_BACKEND")
            pl.pop_profile()
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")
        
        pl.pop_output_binary()
        pl.pop_source_files()

    ###############################################################################
    #                                  examples                                   #
    ###############################################################################

    add_example_app("example_0")
    add_example_app("example_1")
    add_example_app("example_2")
    add_example_app("example_3")
    add_example_app("example_4")
    add_example_app("example_5")
    add_example_app("example_6")
    add_example_app("example_8")

    ###############################################################################
    #                                 pilot_light                                 #
    ###############################################################################
    with pl.target("pilot_light", pl.TargetType.EXECUTABLE):

        pl.push_output_binary("pilot_light")
               
        with pl.configuration("debug"):
            pl.push_profile(pl.Profile.VULKAN)
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pl.add_definition("PL_VULKAN_BACKEND")
                    pl.add_source_file("../src/pl_main_win32.c")
                    pl.add_definition("_DEBUG")          
            with pl.platform(pl.PlatformType.LINUX):
                with pl.compiler("gcc", pl.CompilerType.GCC):
                    pl.add_definition("PL_VULKAN_BACKEND")
                    pl.add_source_file("../src/pl_main_x11.c")
            pl.pop_profile()
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")
                    pl.add_source_file("../src/pl_main_macos.m")
        
        pl.pop_output_binary()

    pl.pop_definitions()
    pl.pop_include_directories()
    pl.pop_link_directories()
    pl.pop_output_directory()  
    pl.pop_profile()

pl.generate_build_script("build")