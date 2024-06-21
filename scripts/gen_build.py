import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/../pl_build")

import pl_build as pl

###############################################################################
#                                helpers                                      #
###############################################################################

def add_plugin_to_vulkan_app(name, reloadable, binary_name = None, directory = "../extensions/", *kwargs):

    with pl.target(name, pl.TargetType.DYNAMIC_LIBRARY, reloadable):
        if binary_name is None:
            pl.push_output_binary(name)
        else:
            pl.push_output_binary(binary_name)
        source_files = [directory + name + ".c"]
        for source in kwargs:
            source_files.append(directory + source + ".c")
        pl.push_source_files(*source_files)
        with pl.configuration("debug"):
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pass
            with pl.platform(pl.PlatformType.LINUX):
                with pl.compiler("gcc", pl.CompilerType.GCC):
                    pass
        with pl.configuration("vulkan"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pass
        pl.pop_output_binary()
        pl.pop_source_files()

def add_plugin_to_metal_app(name, reloadable, objc = False, binary_name = None, directory = "../extensions/"):

    pl.push_definitions("PL_METAL_BACKEND")
    with pl.target(name, pl.TargetType.DYNAMIC_LIBRARY, reloadable):

        if binary_name is None:
            pl.push_output_binary(name)
        else:
            pl.push_output_binary(binary_name)
        if objc:
            pl.push_source_files(directory + name + ".m")
        else:
            pl.push_source_files(directory + name + ".c")
        with pl.configuration("debug"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pass
        pl.pop_output_binary()
        pl.pop_source_files()
    pl.pop_definitions()

pl.register_standard_profiles()

with pl.project("pilotlight"):
    
    # configurations
    pl.add_configuration("debug")
    pl.add_configuration("vulkan") # only used on macos for vulkan

    # where to output build scripts
    pl.set_working_directory(os.path.dirname(os.path.abspath(__file__)) + "/../src")

    # used to decide hot reloading
    pl.set_main_target("pilot_light")

    pl.push_profile(pl.Profile.PILOT_LIGHT_DEBUG_C)

    pl.push_definitions("_USE_MATH_DEFINES", "PL_PROFILING_ON", "PL_ALLOW_HOT_RELOAD", "PL_ENABLE_VALIDATION_LAYERS")
    pl.push_include_directories("../editor", "../src", "../libs", "../extensions", "../out", "../dependencies/stb", "../dependencies/cgltf")
    pl.push_link_directories("../out")
    pl.push_output_directory("../out")
        
    ###############################################################################
    #                            pilotlight_lib                                   #
    ###############################################################################
    with pl.target("pilotlight_lib", pl.TargetType.STATIC_LIBRARY):

        pl.push_source_files("pilotlight_lib.c")
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
        

        with pl.configuration("vulkan"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_VULKAN_BACKEND")

        pl.pop_output_binary()
        pl.pop_source_files()

    ###############################################################################
    #                                   plugins                                   #
    ###############################################################################
    pl.push_target_links("pilotlight_lib")

    add_plugin_to_vulkan_app("pl_draw_ext",     True)
    add_plugin_to_vulkan_app("pl_debug_ext",    False)
    add_plugin_to_vulkan_app("pl_image_ext",    False)
    add_plugin_to_vulkan_app("pl_stats_ext",    False)
    add_plugin_to_vulkan_app("pl_job_ext",    False)
    add_plugin_to_vulkan_app("pl_model_loader_ext", False)
    add_plugin_to_vulkan_app("pl_ecs_ext",      True)
    add_plugin_to_vulkan_app("pl_resource_ext", False)
    add_plugin_to_vulkan_app("pl_gpu_allocators_ext", False)
    add_plugin_to_vulkan_app("pl_ui_ext", True, None)

    pl.push_profile(pl.Profile.VULKAN)
    pl.push_definitions("PL_VULKAN_BACKEND")
    add_plugin_to_vulkan_app("pl_ref_renderer_ext", True)
    add_plugin_to_vulkan_app("pl_graphics_vulkan",   False, "pl_graphics_ext")
    pl.pop_profile()
    pl.pop_definitions()

    add_plugin_to_metal_app("pl_draw_ext",     True)
    add_plugin_to_metal_app("pl_debug_ext",    False)
    add_plugin_to_metal_app("pl_image_ext",    False)
    add_plugin_to_metal_app("pl_stats_ext",    False)
    add_plugin_to_metal_app("pl_job_ext",    False)
    add_plugin_to_metal_app("pl_ecs_ext",      True)
    add_plugin_to_metal_app("pl_model_loader_ext",      False)
    add_plugin_to_metal_app("pl_resource_ext", False)
    add_plugin_to_metal_app("pl_graphics_metal",    False, True, "pl_graphics_ext")
    add_plugin_to_metal_app("pl_gpu_allocators_ext", False)
    add_plugin_to_metal_app("pl_ref_renderer_ext", True)
    add_plugin_to_metal_app("pl_ui_ext", True, False, None)

    ###############################################################################
    #                                 shader ext                                  #
    ###############################################################################
    pl.push_profile(pl.Profile.VULKAN)
    with pl.target("pl_shader_ext", pl.TargetType.DYNAMIC_LIBRARY, False):
        pl.push_output_binary("pl_shader_ext")
        pl.push_source_files("../extensions/pl_shader_ext.c")
        with pl.configuration("debug"):
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pl.add_link_library("shaderc_combined.lib")
            with pl.platform(pl.PlatformType.LINUX):
                with pl.compiler("gcc", pl.CompilerType.GCC):
                    pl.add_link_library("shaderc_shared")
        with pl.configuration("vulkan"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_link_library("shaderc_shared")
        pl.pop_output_binary()
        pl.pop_source_files()
    pl.pop_profile()

    pl.push_definitions("PL_METAL_BACKEND")
    with pl.target("pl_shader_ext", pl.TargetType.DYNAMIC_LIBRARY, False):
        pl.push_output_binary("pl_shader_ext")
        pl.push_source_files("../extensions/pl_shader_ext.c")
        with pl.configuration("debug"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_link_library("spirv-cross-c-shared")
                    pl.add_link_library("shaderc_shared")
        pl.pop_output_binary()
        pl.pop_source_files()
    pl.pop_definitions()

    # scripts
    add_plugin_to_vulkan_app("pl_script_camera", False, None)
    add_plugin_to_metal_app("pl_script_camera", False, False, None)

    pl.pop_target_links()

    ###############################################################################
    #                                 editor                                      #
    ###############################################################################

    with pl.target("editor", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.push_output_binary("editor")
        pl.push_target_links("pilotlight_lib")
        pl.push_source_files("../editor/editor.c")
        
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
        
        pl.push_profile(pl.Profile.VULKAN)
        with pl.configuration("vulkan"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_VULKAN_BACKEND")
        pl.pop_profile()

        pl.pop_source_files()
        pl.pop_output_binary()
        pl.pop_target_links()

    ###############################################################################
    #                                  app                                        #
    ###############################################################################

    with pl.target("app", pl.TargetType.DYNAMIC_LIBRARY, False):

        pl.push_output_binary("app")
        pl.push_target_links("pilotlight_lib")
        pl.push_source_files("../editor/app.c")
        
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
                    pl.add_source_file("pl_main_win32.c")
                    pl.add_definition("_DEBUG")          
            with pl.platform(pl.PlatformType.LINUX):
                with pl.compiler("gcc", pl.CompilerType.GCC):
                    pl.add_definition("PL_VULKAN_BACKEND")
                    pl.add_source_file("pl_main_x11.c")
            pl.pop_profile() 
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")
                    pl.add_source_file("pl_main_macos.m")
        
        with pl.configuration("vulkan"):
            pl.push_profile(pl.Profile.VULKAN)
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_VULKAN_BACKEND")
                    pl.add_source_file("pl_main_macos.m")
            pl.pop_profile() 

        pl.pop_output_binary()

    pl.pop_definitions()
    pl.pop_include_directories()
    pl.pop_link_directories()
    pl.pop_output_directory()  
    pl.pop_profile()

pl.generate_build_script("build")
