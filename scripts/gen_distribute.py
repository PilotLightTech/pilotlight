# gen_distribute.py

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

import build.core as pl
import build.backend_win32 as win32
import build.backend_linux as linux
import build.backend_macos as apple

#-----------------------------------------------------------------------------
# [SECTION] project
#-----------------------------------------------------------------------------

# where to output build scripts
working_directory = os.path.dirname(os.path.abspath(__file__)) + "/../src"

with pl.project("pilotlight deploy"):
    
    # project wide settings
    pl.set_output_directory("../out")
    pl.add_link_directories("../out")
    pl.add_include_directories("../editor", "../src", "../shaders", "../libs", "../extensions", "../out", "../dependencies/stb", "../dependencies/cgltf")

    #-----------------------------------------------------------------------------
    # [SECTION] profiles
    #-----------------------------------------------------------------------------

    # win32 or msvc only
    pl.add_profile(platform_filter=["Windows"],
                    include_directories=['%WindowsSdkDir%Include\\um', '%WindowsSdkDir%Include\\shared'])
    pl.add_profile(compiler_filter=["msvc"],
                    target_type_filter=[pl.TargetType.DYNAMIC_LIBRARY],
                    linker_flags=["-noimplib", "-noexp"])
    
    pl.add_profile(compiler_filter=["msvc"],
                    linker_flags=["-incremental:no"],
                    compiler_flags=["-Zc:preprocessor", "-nologo", "-std:c11", "-W4", "-WX", "-wd4201",
                                "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115", "-permissive-"])
    pl.add_profile(compiler_filter=["msvc"],
                    configuration_filter=["debug"],
                    compiler_flags=["-Od", "-MDd", "-Zi"])
    pl.add_profile(compiler_filter=["msvc"],
                    configuration_filter=["release"],
                    compiler_flags=["-O2", "-MD"])


    # linux or gcc only
    pl.add_profile(platform_filter=["Linux"],
                    link_directories=["/usr/lib/x86_64-linux-gnu"])
    pl.add_profile(compiler_filter=["gcc"],
                    linker_flags=["-ldl", "-lm"],
                    compiler_flags=["-std=gnu11", "-fPIC"])
    pl.add_profile(compiler_filter=["gcc"],
                    configuration_filter=["debug"],
                    compiler_flags=["--debug", "-g"])

    # macos or clang only
    pl.add_profile(platform_filter=["Darwin"],
                    link_frameworks=["Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore"])
    pl.add_profile(compiler_filter=["clang"],
                    link_directories=["/usr/local/lib"],
                    compiler_flags=["-std=c99", "-fmodules", "-ObjC", "-fPIC"])
    pl.add_profile(compiler_filter=["clang"],
                    configuration_filter=["debug"],
                    compiler_flags=["--debug", "-g"])

    # graphics backends
    pl.add_profile(configuration_filter=["debug", "release"], compiler_filter=["gcc", "msvc"],
                    definitions=["PL_VULKAN_BACKEND"])
    pl.add_profile(configuration_filter=["debug", "release"], platform_filter=["Darwin"],
                    definitions=["PL_METAL_BACKEND"])
    
    # configs
    pl.add_profile(configuration_filter=["debug"], definitions=["_DEBUG", "PL_CONFIG_DEBUG"])
    pl.add_profile(configuration_filter=["release"], definitions=["NDEBUG", "PL_CONFIG_RELEASE"])
                    
    #-----------------------------------------------------------------------------
    # [SECTION] extensions
    #-----------------------------------------------------------------------------

    extensions = [
        "pl_profile_ext",
        "pl_image_ext",
        "pl_stats_ext",
        "pl_rect_pack_ext",
        "pl_string_intern_ext",
        "pl_draw_ext",
        "pl_job_ext",
        "pl_log_ext",
        "pl_gpu_allocators_ext",
        "pl_ecs_ext",
        "pl_tools_ext",
        "pl_renderer_ext",
        "pl_resource_ext",
        "pl_model_loader_ext",
        "pl_ui_ext",
        "pl_ecs_tools_ext",
        "pl_camera_ext",
        "pl_animation_ext",
        "pl_gizmo_ext",
        "pl_console_ext",
        "pl_screen_log_ext",
        "pl_starter_ext",
        "pl_physics_ext",
        "pl_collision_ext",
        "pl_bvh_ext",
        "pl_config_ext",
        "pl_mesh_ext",
        "pl_shader_variant_ext",
        "pl_datetime_ext",
        "pl_vfs_ext",
        "pl_compress_ext",
        "pl_dds_ext",
        "pl_dxt_ext",
        "pl_pak_ext",
        "pl_script_ext",
        "pl_material_ext",
        "pl_terrain_ext",
        "pl_terrain_processor_ext",
        "pl_freelist_ext",
        "pl_image_ops_ext",
    ]

    for extension in extensions:

        with pl.target(extension, pl.TargetType.DYNAMIC_LIBRARY, False):

            pl.add_source_files("../extensions/" + extension + ".c")
            
            # default config
            with pl.configuration("debug"):

                pl.set_output_binary(extension + "d")

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

            # release
            with pl.configuration("release"):

                pl.set_output_binary(extension)

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
    
    #-----------------------------------------------------------------------------
    # [SECTION] extensions
    #-----------------------------------------------------------------------------

    with pl.target("pl_graphics_ext", pl.TargetType.DYNAMIC_LIBRARY, False):

        pl.add_source_files("../extensions/pl_graphics_ext.c")

        # default config
        with pl.configuration("debug"):

            pl.set_output_binary("pl_graphics_extd")

            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_static_link_libraries("vulkan-1")
                    # pl.add_linker_flags("-nodefaultlib:MSVCRT")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_dynamic_link_libraries("vulkan")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-Wno-deprecated-declarations")

        # release
        with pl.configuration("release"):

            pl.set_output_binary("pl_graphics_ext")

            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_static_link_libraries("vulkan-1")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_dynamic_link_libraries("vulkan")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-Wno-deprecated-declarations")

    #-----------------------------------------------------------------------------
    # [SECTION] extensions
    #-----------------------------------------------------------------------------

    with pl.target("pl_shader_ext", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.add_source_files("../extensions/pl_shader_ext.c")
        

        # default config
        with pl.configuration("debug"):

            pl.set_output_binary("pl_shader_extd")

            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_static_link_libraries("spirv-cross-c-shared", "shaderc_combined")
                    pl.add_linker_flags("-nodefaultlib:MSVCRT")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared")

        # release
        with pl.configuration("release"):

            pl.set_output_binary("pl_shader_ext")

            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("spirv-cross-c-shared", "shaderc_combined")
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared")

    #-----------------------------------------------------------------------------
    # [SECTION] platform extension
    #-----------------------------------------------------------------------------

    with pl.target("pl_platform_ext", pl.TargetType.DYNAMIC_LIBRARY, False):
    
        # default config
        with pl.configuration("debug"):

            pl.set_output_binary("pl_platform_extd")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_source_files("../extensions/pl_platform_win32_ext.c")
                    pl.add_static_link_libraries("ucrtd", "user32", "Ole32")
                       
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("../extensions/pl_platform_linux_ext.c")
                    pl.add_dynamic_link_libraries("pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../extensions/pl_platform_macos_ext.m")
        
        # release
        with pl.configuration("release"):

            pl.set_output_binary("pl_platform_ext")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_source_files("../extensions/pl_platform_win32_ext.c")
                    pl.add_static_link_libraries("ucrt", "user32", "Ole32")
                    

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("../extensions/pl_platform_linux_ext.c")
                    pl.add_dynamic_link_libraries("pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../extensions/pl_platform_macos_ext.m")

    #-----------------------------------------------------------------------------
    # [SECTION] scripts
    #-----------------------------------------------------------------------------

    # vulkan backend
    with pl.target("pl_script_camera", pl.TargetType.DYNAMIC_LIBRARY, False):

        pl.add_source_files("../extensions/pl_script_camera.c")
        

        # default config
        with pl.configuration("debug"):

            pl.set_output_binary("pl_script_camerad")

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

        # release
        with pl.configuration("release"):

            pl.set_output_binary("pl_script_camera")

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

    #-----------------------------------------------------------------------------
    # [SECTION] pilot_light
    #-----------------------------------------------------------------------------

    with pl.target("pilot_light", pl.TargetType.EXECUTABLE):
    
        # default config
        with pl.configuration("debug"):

            pl.set_output_binary("pilot_lightd")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("ucrtd", "user32", "Ole32")
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
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
        
        # release
        with pl.configuration("release"):

            pl.set_output_binary("pilot_light")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("ucrt", "user32", "Ole32")
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
                    pl.add_compiler_flags("-Wno-deprecated-declarations")

#-----------------------------------------------------------------------------
# [SECTION] generate scripts
#-----------------------------------------------------------------------------

if plat.system() == "Windows":
    win32.generate_build(working_directory + '/' + "build_distribute.bat")
elif plat.system() == "Darwin":
    apple.generate_build(working_directory + '/' + "build_distribute.sh")
elif plat.system() == "Linux":
    linux.generate_build(working_directory + '/' + "build_distribute.sh")
