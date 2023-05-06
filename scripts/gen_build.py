import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/../tools")

import pl_build as pl

###############################################################################
#                                helpers                                      #
###############################################################################

def add_plugin_to_vulkan_app(name, reloadable, *kwargs):
    with pl.target(name, pl.TargetType.DYNAMIC_LIBRARY, reloadable):
        pl.push_output_binary(name)
        source_files = ["../extensions/" + name + ".c"]
        for source in kwargs:
            source_files.append("../extensions/" + source + ".c")
        pl.push_source_files(*source_files)
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
        pl.pop_output_binary()
        pl.pop_source_files()

def add_plugin_to_metal_app(name, reloadable):
    with pl.target(name, pl.TargetType.DYNAMIC_LIBRARY, reloadable):
        pl.push_output_binary(name)
        pl.push_source_files("../extensions/" + name + ".c")
        with pl.configuration("debugmetal"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")
        pl.pop_output_binary()
        pl.pop_source_files()

pl.register_standard_profiles()

with pl.project("pilotlight"):
    
    # configurations
    pl.add_configuration("debug")
    pl.add_configuration("debugmetal") # only used on macos for vulkan

    # where to output build scripts
    pl.set_working_directory(os.path.dirname(os.path.abspath(__file__)) + "/../src")

    # used to decide hot reloading
    pl.set_main_target("pilot_light")

    pl.push_profile(pl.Profile.PILOT_LIGHT_DEBUG_C)

    pl.push_definitions("_USE_MATH_DEFINES", "PL_PROFILING_ON", "PL_ALLOW_HOT_RELOAD", "PL_ENABLE_VALIDATION_LAYERS")
    pl.push_include_directories("../apps", "../src", "../libs", "../extensions", "../backends", "../out", "../dependencies/stb")
    pl.push_link_directories("../out")
    pl.push_output_directory("../out")
        
    ###############################################################################
    #                            pilotlight_lib                                   #
    ###############################################################################
    with pl.target("pilotlight_lib", pl.TargetType.STATIC_LIBRARY):

        pl.push_source_files("pilotlight_lib.c")
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

        with pl.configuration("debugmetal"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")

        pl.pop_output_binary()
        pl.pop_source_files()

    ###############################################################################
    #                                   plugins                                   #
    ###############################################################################
    pl.push_target_links("pilotlight_lib")
    
    pl.push_profile(pl.Profile.VULKAN)
    pl.push_definitions("PL_VULKAN_BACKEND")
    add_plugin_to_vulkan_app("pl_draw_ext", True)
    add_plugin_to_vulkan_app("pl_ui_ext", True)
    add_plugin_to_vulkan_app("pl_image_ext", False)
    add_plugin_to_vulkan_app("pl_renderer_ext", False)
    add_plugin_to_vulkan_app("pl_ecs_ext", False)
    add_plugin_to_vulkan_app("pl_vulkan_ext", False)
    add_plugin_to_vulkan_app("pl_gltf_ext", False)
    pl.pop_profile()
    pl.pop_definitions()

    pl.push_definitions("PL_METAL_BACKEND")
    add_plugin_to_metal_app("pl_draw_ext", True)
    add_plugin_to_metal_app("pl_ui_ext", True)
    add_plugin_to_metal_app("pl_image_ext", False)
    pl.pop_definitions()

    pl.pop_target_links()

    ###############################################################################
    #                                    app                                      #
    ###############################################################################
    with pl.target("app", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.push_output_binary("app")
        pl.push_target_links("pilotlight_lib")

        pl.push_profile(pl.Profile.VULKAN)
        pl.push_definitions("PL_VULKAN_BACKEND")
        pl.push_source_files("../apps/app_vulkan.c")
        pl.push_vulkan_glsl_files("../shaders/glsl/", "phong.frag", "primitive.vert", "skybox.frag", "skybox.vert")
        pl.push_vulkan_glsl_files("../shaders/glsl/", "grass.frag", "grass.vert", "outline.frag", "outline.vert")
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
        pl.pop_vulkan_glsl_files() 
        pl.pop_vulkan_glsl_files() 

        with pl.configuration("debugmetal"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")
                    pl.add_source_file("../apps/app_metal.m")

        pl.pop_output_binary()
        pl.pop_target_links()

    ###############################################################################
    #                                 pilot_light                                 #
    ###############################################################################
    with pl.target("pilot_light", pl.TargetType.EXECUTABLE):

        pl.push_output_binary("pilot_light")
               
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

        with pl.configuration("debugmetal"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")
                    pl.add_source_file("pl_main_macos.m")

        pl.pop_output_binary()

    pl.pop_definitions()
    pl.pop_include_directories()
    pl.pop_link_directories()
    pl.pop_output_directory()  
    pl.pop_profile()

pl.generate_build_script("build")