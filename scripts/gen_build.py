import os
import sys
import shutil

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/../dependencies/pilotlight-build/pilotlight_build")

import pl_build as pl

###############################################################################
#                                helpers                                      #
###############################################################################

def add_plugin_to_vulkan_app(name, reloadable, binary_name = None, *kwargs):

    pl.push_profile(pl.Profile.VULKAN)
    pl.push_definitions("PL_VULKAN_BACKEND")
    with pl.target(name, pl.TargetType.DYNAMIC_LIBRARY, reloadable):
        if binary_name is None:
            pl.push_output_binary(name)
        else:
            pl.push_output_binary(binary_name)
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
        with pl.configuration("vulkan"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pass
        pl.pop_output_binary()
        pl.pop_source_files()
    pl.pop_profile()
    pl.pop_definitions()

def add_plugin_to_metal_app(name, reloadable, objc = False, binary_name = None):

    pl.push_definitions("PL_METAL_BACKEND")
    with pl.target(name, pl.TargetType.DYNAMIC_LIBRARY, reloadable):

        if binary_name is None:
            pl.push_output_binary(name)
        else:
            pl.push_output_binary(binary_name)
        if objc:
            pl.push_source_files("../extensions/" + name + ".m")
        else:
            pl.push_source_files("../extensions/" + name + ".c")
        with pl.configuration("debug"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")
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
    pl.push_include_directories("../apps", "../src", "../extensions", "../backends", "../out", "../dependencies/pilotlight-ui", "../dependencies/pilotlight-ui/backends", "../dependencies/pilotlight-libs", "../dependencies/stb", "../dependencies/cgltf")
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
    
    add_plugin_to_vulkan_app("pl_debug_ext",    False)
    add_plugin_to_vulkan_app("pl_image_ext",    False)
    add_plugin_to_vulkan_app("pl_vulkan_ext",   False, "pl_graphics_ext")
    add_plugin_to_vulkan_app("pl_stats_ext",    False)
    add_plugin_to_vulkan_app("pl_ecs_ext",      False)
    add_plugin_to_vulkan_app("pl_resource_ext", False)
    add_plugin_to_vulkan_app("pl_ref_renderer_ext", True)

    add_plugin_to_metal_app("pl_debug_ext",    False)
    add_plugin_to_metal_app("pl_image_ext",    False)
    add_plugin_to_metal_app("pl_stats_ext",    False)
    add_plugin_to_metal_app("pl_ecs_ext",      False)
    add_plugin_to_metal_app("pl_resource_ext", False)
    add_plugin_to_metal_app("pl_metal_ext",    False, True, "pl_graphics_ext")
    add_plugin_to_metal_app("pl_ref_renderer_ext", True)

    pl.pop_target_links()

    ###############################################################################
    #                                    app                                      #
    ###############################################################################

    vulkan_shaders = [
        "skybox.frag",
        "skybox.vert",
        "primitive.frag",
        "primitive.vert",
        "lighting.vert",
        "lighting.frag",
        "draw_3d.vert",
        "draw_3d.frag",
        "draw_3d_line.vert",
        "panorama_to_cubemap.comp",
    ]

    metal_shaders = [
        "panorama_to_cubemap.metal",
        "draw_3d_line.metal",
        "draw_3d.metal",
        "primitive.metal",
        "lighting.metal",
        "skybox.metal"
    ]

    with pl.target("app", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.push_output_binary("app")
        pl.push_target_links("pilotlight_lib")

        if os.path.isfile('../apps/app_user.c'):
            pl.push_source_files("../apps/app_user.c")
        else:
            pl.push_source_files("../apps/app.c")
        
        with pl.configuration("debug"):
            pl.push_profile(pl.Profile.VULKAN)
            pl.push_vulkan_glsl_files("../shaders/glsl/", *vulkan_shaders)
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pl.add_definition("PL_VULKAN_BACKEND")
            with pl.platform(pl.PlatformType.LINUX):
                with pl.compiler("gcc", pl.CompilerType.GCC):
                    pl.add_definition("PL_VULKAN_BACKEND")
            pl.pop_profile() 
            pl.pop_vulkan_glsl_files()
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_METAL_BACKEND")
                    pl.add_metal_files("../shaders/metal/", *metal_shaders)
        
        
        pl.push_profile(pl.Profile.VULKAN)
        pl.push_vulkan_glsl_files("../shaders/glsl/",  *vulkan_shaders)
        with pl.configuration("vulkan"):
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pl.add_definition("PL_VULKAN_BACKEND")
        pl.pop_profile() 
        pl.pop_vulkan_glsl_files()

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
                    pl.add_source_file("pl_main_linux.c")
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
if not os.path.isdir("../out"):
    os.mkdir("../out")
shutil.copy("../apps/pl_config.json", "../out/pl_config.json")