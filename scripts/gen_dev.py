# gen_dev.py

# Index of this file:
# [SECTION] imports
# [SECTION] project
# [SECTION] profiles
# [SECTION] extensions
# [SECTION] ecs scripts
# [SECTION] platform extension
# [SECTION] graphics extension
# [SECTION] app
# [SECTION] pilot_light
# [SECTION] imgui & implot
# [SECTION] editor app
# [SECTION] pl_dear_imgui_ext
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

output_directory = "../out"

if len(sys.argv) > 1:
    output_directory = sys.argv[1]

with pl.project("pilotlight"):
    
    # used to decide hot reloading
    pl.add_hot_reload_target(output_directory + "/pilot_light")
    pl.set_hot_reload_artifact_directory(output_directory + "/../out-temp")

    # project wide settings
    pl.set_output_directory(output_directory)
    pl.add_link_directories(output_directory)
    pl.add_include_directories("../sandbox", "../src", "../shaders", "../libs", "../extensions", output_directory, "../thirdparty/stb",
                               "../thirdparty/cgltf", "../thirdparty/imgui")

    #-----------------------------------------------------------------------------
    # [SECTION] profiles
    #-----------------------------------------------------------------------------

    pl.add_profile(platform_filter=["Windows"], definitions=["PL_PLATFORM_WINDOWS"])
    pl.add_profile(platform_filter=["Linux"], definitions=["PL_PLATFORM_LINUX"])
    pl.add_profile(platform_filter=["Darwin"], definitions=["PL_PLATFORM_APPLE"])

    # win32 or msvc only
    pl.add_profile(platform_filter=["Windows"],
                    include_directories=['%WindowsSdkDir%Include\\um', '%WindowsSdkDir%Include\\shared'])
    pl.add_profile(compiler_filter=["msvc"],
                    target_type_filter=[pl.TargetType.DYNAMIC_LIBRARY],
                    linker_flags=["-noexp"])
    pl.add_profile(compiler_filter=["msvc"],
                    linker_flags=["-incremental:no"],
                    compiler_flags=["-Zc:preprocessor", "-nologo", "-W4", "-WX", "-wd4201",
                                "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115", "-permissive-"])
    pl.add_profile(compiler_filter=["msvc"],
                    configuration_filter=["debug", "test"],
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
                    configuration_filter=["debug", "test"],
                    compiler_flags=["--debug", "-g"])

    # macos or clang only
    pl.add_profile(platform_filter=["Darwin"],
                    link_frameworks=["Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore"])
    pl.add_profile(compiler_filter=["clang"],
                    link_directories=["/usr/local/lib"],
                    compiler_flags=["-std=c99", "-fmodules", "-ObjC", "-fPIC"])
    pl.add_profile(compiler_filter=["clang"],
                    configuration_filter=["debug", "moltenvk", "test"],
                    compiler_flags=["--debug", "-g"])
    
    # configs
    pl.add_profile(configuration_filter=["test"], definitions=["PL_CONFIG_TEST"])
    pl.add_profile(configuration_filter=["debug", "moltenvk", "test"], definitions=["_DEBUG", "PL_CONFIG_DEBUG"])
    pl.add_profile(configuration_filter=["release"], definitions=["NDEBUG", "PL_CONFIG_RELEASE"])
                    
    #-----------------------------------------------------------------------------
    # [SECTION] extensions
    #-----------------------------------------------------------------------------

    with pl.target("pl_unity_ext", pl.TargetType.DYNAMIC_LIBRARY, reloadable=True):

        pl.add_source_files("../extensions/pl_unity_ext.c")
        pl.set_output_binary("pl_unity_ext")

        def add_debug_config():

            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    pl.add_linker_flags("-nodefaultlib:MSVCRT")
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")

        with pl.configuration("debug"): add_debug_config()
        with pl.configuration("test"):  add_debug_config()
        
        with pl.configuration("release"):

            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")

        # vulkan on macos
        with pl.configuration("moltenvk"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_linker_flags("-lstdc++")
                    pl.add_dynamic_link_libraries("pthread")

    #-----------------------------------------------------------------------------
    # [SECTION] shader extension
    #-----------------------------------------------------------------------------

    with pl.target("pl_shader_ext", pl.TargetType.DYNAMIC_LIBRARY, reloadable=True):

        pl.add_source_files("../extensions/pl_shader_ext.c")
        pl.set_output_binary("pl_shader_ext")

        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                pl.add_definitions("PL_VULKAN_BACKEND")
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_linker_flags("-nodefaultlib:MSVCRT")
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                pl.add_definitions("PL_VULKAN_BACKEND")
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries( "xcb", "X11", "X11-xcb",
                                                    "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread")
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_linker_flags("-lstdc++")

            # macos
            with pl.platform("Darwin"):
                pl.add_definitions("PL_METAL_BACKEND")
                with pl.compiler("clang"):
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
        
        with pl.configuration("release"):

            # win32
            with pl.platform("Windows"):
                pl.add_definitions("PL_VULKAN_BACKEND")
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                pl.add_definitions("PL_VULKAN_BACKEND")
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb",
                                                    "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread")
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_linker_flags("-lstdc++")

            # macos
            with pl.platform("Darwin"):
                pl.add_definitions("PL_METAL_BACKEND")
                with pl.compiler("clang"):
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')

        # vulkan on macos
        with pl.configuration("moltenvk"):
            pl.add_definitions("PL_VULKAN_BACKEND")
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_linker_flags("-lstdc++")
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")

        with pl.configuration("test"):

            pl.add_definitions("PL_CPU_BACKEND", "PL_OFFLINE_SHADERS_ONLY")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-nodefaultlib:MSVCRT")
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")

    #-----------------------------------------------------------------------------
    # [SECTION] cpu shader extension
    #-----------------------------------------------------------------------------

    with pl.target("pl_shader_cpu_ext", pl.TargetType.DYNAMIC_LIBRARY, cache=True, max_cache_age_mins=10):

        pl.add_source_files("../extensions/pl_shader_ext.c")
        pl.set_output_binary("pl_shader_cpu_ext")
        pl.add_definitions("PL_CPU_BACKEND", "PL_OFFLINE_SHADERS_ONLY")

        def add_debug_config():

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-nodefaultlib:MSVCRT")
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")

        with pl.configuration("debug"): add_debug_config()
        with pl.configuration("test"):  add_debug_config()
        
        with pl.configuration("release"):

            # win32
            with pl.platform("Windows"):
                
                with pl.compiler("msvc"):
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")

        # vulkan on macos
        with pl.configuration("moltenvk"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_linker_flags("-lstdc++")
                    pl.add_dynamic_link_libraries("pthread")

    #-----------------------------------------------------------------------------
    # [SECTION] graphics extension
    #-----------------------------------------------------------------------------

    with pl.target("pl_graphics_ext", pl.TargetType.DYNAMIC_LIBRARY, reloadable=True):

        pl.add_source_files("../extensions/pl_graphics_ext.c")
        pl.set_output_binary("pl_graphics_ext")

        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                pl.add_definitions("PL_VULKAN_BACKEND")
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("vulkan-1")
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_linker_flags("-nodefaultlib:MSVCRT")
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                pl.add_definitions("PL_VULKAN_BACKEND")
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries( "xcb", "X11", "X11-xcb",
                                                    "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread",
                                                    "vulkan")
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_linker_flags("-lstdc++")

            # macos
            with pl.platform("Darwin"):
                pl.add_definitions("PL_METAL_BACKEND")
                with pl.compiler("clang"):
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
        
        with pl.configuration("release"):

            # win32
            with pl.platform("Windows"):
                pl.add_definitions("PL_VULKAN_BACKEND")
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("vulkan-1")
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                pl.add_definitions("PL_VULKAN_BACKEND")
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb",
                                                    "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread",
                                                    "vulkan")
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_linker_flags("-lstdc++")

            # macos
            with pl.platform("Darwin"):
                pl.add_definitions("PL_METAL_BACKEND")
                with pl.compiler("clang"):
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')

        # vulkan on macos
        with pl.configuration("moltenvk"):
            pl.add_definitions("PL_VULKAN_BACKEND")
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_linker_flags("-lstdc++")
                    pl.add_dynamic_link_libraries("pthread", "vulkan")
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")

        with pl.configuration("test"):

            pl.add_definitions("PL_CPU_BACKEND", "PL_OFFLINE_SHADERS_ONLY")

            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    pl.add_linker_flags("-nodefaultlib:MSVCRT")
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries( "xcb", "X11", "X11-xcb",
                                                    "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread")
                    pl.add_linker_flags("-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")

    #-----------------------------------------------------------------------------
    # [SECTION] cpu graphics extension
    #-----------------------------------------------------------------------------

    with pl.target("pl_graphics_cpu_ext", pl.TargetType.DYNAMIC_LIBRARY, cache=True, max_cache_age_mins=10):

        pl.add_source_files("../extensions/pl_graphics_ext.c")
        pl.set_output_binary("pl_graphics_cpu_ext")
        pl.add_definitions("PL_CPU_BACKEND", "PL_OFFLINE_SHADERS_ONLY")

        
        def add_debug_config():

            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    pl.add_linker_flags("-nodefaultlib:MSVCRT")
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries( "xcb", "X11", "X11-xcb",
                                                    "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread")
                    pl.add_linker_flags("-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")

        with pl.configuration("debug"): add_debug_config()
        with pl.configuration("test"):  add_debug_config()
        with pl.configuration("release"):

            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")

        # vulkan on macos
        with pl.configuration("moltenvk"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_linker_flags("-lstdc++")
                    pl.add_dynamic_link_libraries("pthread")
                    
    #-----------------------------------------------------------------------------
    # [SECTION] ecs scripts
    #-----------------------------------------------------------------------------

    with pl.target("pl_script_camera", pl.TargetType.DYNAMIC_LIBRARY, reloadable=True):

        pl.set_output_binary("pl_script_camera")
        pl.add_source_files("../extensions/pl_script_camera.c")

        def add_script_ext():

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_compiler_flags("-std:c11")
                    pl.add_linker_flags("-noimplib")
                                
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pass

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

        with pl.configuration("debug"):     add_script_ext()
        with pl.configuration("release"):   add_script_ext()
        with pl.configuration("test"):      add_script_ext()

        # vulkan on macos
        with pl.configuration("moltenvk"):

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

    #-----------------------------------------------------------------------------
    # [SECTION] platform extension
    #-----------------------------------------------------------------------------

    with pl.target("pl_platform_ext", pl.TargetType.DYNAMIC_LIBRARY, cache=True, max_cache_age_mins=10):
    
        pl.set_output_binary("pl_platform_ext")

        with pl.configuration("debug"): 

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_source_files("../extensions/pl_platform_win32_ext.c")
                    pl.add_static_link_libraries("user32", "Ole32", "gdi32")
                    pl.add_compiler_flags("-std:c11")
                        
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("../extensions/pl_platform_x11_ext.c")
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes",
                                                  "xcb-keysyms", "pthread")
                    pl.add_compiler_flags("-std=gnu11", "-fPIC", "--debug", "-g")
                    pl.add_linker_flags("-ldl", "-lm")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../extensions/pl_platform_macos_ext.m")

        with pl.configuration("release"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_source_files("../extensions/pl_platform_win32_ext.c")
                    pl.add_static_link_libraries("ucrt", "user32", "Ole32", "gdi32")
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("../extensions/pl_platform_x11_ext.c")
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes",
                                                  "xcb-keysyms", "pthread")
                    pl.add_compiler_flags("-std=gnu11", "-fPIC")
                    pl.add_linker_flags("-ldl", "-lm")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../extensions/pl_platform_macos_ext.m")

        # vulkan on macos
        with pl.configuration("moltenvk"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../extensions/pl_platform_macos_ext.m")

        with pl.configuration("test"):

            pl.add_source_files("../extensions/pl_platform_null_ext.c")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("user32", "Ole32", "gdi32")
                    pl.add_compiler_flags("-std:c11")
                        
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes",
                                                  "xcb-keysyms", "pthread")
                    pl.add_compiler_flags("-std=gnu11", "-fPIC", "--debug", "-g")
                    pl.add_linker_flags("-ldl", "-lm")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

    #-----------------------------------------------------------------------------
    # [SECTION] pilot_light
    #-----------------------------------------------------------------------------

    with pl.target("pilot_light", pl.TargetType.EXECUTABLE):
    
        pl.set_output_binary("pilot_light")
        
        with pl.configuration("debug"):
            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("user32", "Ole32")
                    pl.add_source_files("pl_main_win32.c")
                    pl.add_compiler_flags("-std:c11")
                    
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("pl_main_linux.c")
                    pl.add_dynamic_link_libraries("pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("pl_main_macos.m")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
        with pl.configuration("test"):
            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("user32", "Ole32")
                    pl.add_source_files("pl_main_win32.c")
                    pl.add_compiler_flags("-std:c11")
                    
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("pl_main_linux.c")
                    pl.add_dynamic_link_libraries("pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("pl_main_macos.c")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")

        # release
        with pl.configuration("release"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("ucrt", "user32", "Ole32")
                    pl.add_source_files("pl_main_win32.c")
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("pl_main_linux.c")
                    pl.add_dynamic_link_libraries("pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("pl_main_macos.m")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")

        # vulkan on macos
        with pl.configuration("moltenvk"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("pl_main_macos.m")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")

    # remove all the profiles so settings are explicit
    pl.stash_profiles()

    #-----------------------------------------------------------------------------
    # [SECTION] cpu shaders
    #-----------------------------------------------------------------------------

    shaders = [
        'pl_draw_2d_frag',
        'pl_draw_2d_sdf_frag',
        'pl_draw_2d_vert'
    ]

    for shader in shaders:

        with pl.target(shader, pl.TargetType.DYNAMIC_LIBRARY, cache=True, max_cache_age_mins=10):

            pl.add_source_files("../shaders/" + shader + ".cpp")
            pl.set_output_binary(shader)
            pl.add_definitions("_USE_MATH_DEFINES", "PL_CPU_BACKEND", "PL_SHADER_CODE")
            pl.add_include_directories("../src", "../shaders", "../libs", "../extensions")

            def add_debug_config():

                with pl.platform("Windows"):
                    with pl.compiler("msvc"):
                        pl.add_linker_flags("-noimplib", "-noexp", "-incremental:no")
                        pl.add_compiler_flags("-Zc:preprocessor", "-nologo", "-std:c++14", "-W4", "-WX", "-wd4201",
                                                "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                                "-permissive-", "-Od", "-MDd", "-Zi", "-TP")
                        
                with pl.platform("Linux"):
                    with pl.compiler("gcc"):
                        pl.add_compiler_flags("-std=c++14", "-fPIC", "--debug", "-g")
                        pl.add_linker_flags("-lstdc++", "-ldl", "-lm")
                
                with pl.platform("Darwin"):
                    with pl.compiler("clang"):
                        pl.add_linker_flags("-lstdc++", "-ldl", "-lm")
                        pl.add_compiler_flags("-std=c++14", "--debug", "-g", "-fmodules", "-fPIC")

            with pl.configuration("debug"): add_debug_config()
            with pl.configuration("test"):  add_debug_config()
            with pl.configuration("release"):

                with pl.platform("Windows"):
                    with pl.compiler("msvc"):
                        pl.add_linker_flags("-noimplib", "-noexp", "-incremental:no")
                        pl.add_compiler_flags("-Zc:preprocessor", "-nologo", "-std:c++14", "-W4", "-WX", "-wd4201",
                                                "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                                "-permissive-", "-O2", "-MD", "-Zi", "-TP")
                        
                with pl.platform("Linux"):
                    with pl.compiler("gcc"):
                        pl.add_compiler_flags("-std=c++14", "-fPIC")
                        pl.add_linker_flags("-lstdc++", "-ldl", "-lm")
                
                with pl.platform("Darwin"):
                    with pl.compiler("clang"):
                        pl.add_linker_flags("-lstdc++", "-ldl", "-lm")
                        pl.add_compiler_flags("-std=c++14", "-fmodules", "-fPIC")

    #-----------------------------------------------------------------------------
    # [SECTION] imgui & implot
    #-----------------------------------------------------------------------------

    with pl.target("imgui", pl.TargetType.STATIC_LIBRARY, cache=True, max_cache_age_mins=30):

        # imgui & imgui
        pl.add_source_files("../thirdparty/imgui/imgui_unity.cpp")

        # default config
        def add_debug_config():

            pl.set_output_binary("dearimguid")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_definitions("PL_PLATFORM_WINDOWS")
                    pl.add_linker_flags("-incremental:no", "-nologo")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-WX", "-Od", "-MDd", "-Zi", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_definitions("PL_PLATFORM_LINUX")
                    pl.add_compiler_flags("-fPIC", "-std=c++14", "--debug -g")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("PL_PLATFORM_APPLE")
                    pl.add_compiler_flags("-fPIC", "-std=c++14", "--debug -g")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

        with pl.configuration("debug"): add_debug_config()
        with pl.configuration("test"):  add_debug_config()

        with pl.configuration("release"):

            pl.set_output_binary("dearimgui")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_definitions("PL_PLATFORM_WINDOWS")
                    pl.add_linker_flags("-incremental:no", "-nologo")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-WX", "-O2", "-MD", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_definitions("PL_PLATFORM_LINUX")
                    pl.add_compiler_flags("-fPIC", "-std=c++14")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("PL_PLATFORM_APPLE")
                    pl.add_compiler_flags("-fPIC", "-std=c++14")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

        with pl.configuration("moltenvk"):

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("PL_PLATFORM_APPLE")
                    pl.set_output_binary("dearimguid")
                    pl.add_compiler_flags("-fPIC", "-std=c++14", "--debug -g")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

    #-----------------------------------------------------------------------------
    # [SECTION] pl_dear_imgui_ext
    #-----------------------------------------------------------------------------

    with pl.target("pl_dear_imgui_ext", pl.TargetType.DYNAMIC_LIBRARY, cache=True, max_cache_age_mins=30):

        pl.add_source_files("../extensions/pl_dear_imgui_ext.cpp")
        pl.set_output_binary("pl_dear_imgui_ext")

        def add_debug_config():

            pl.add_definitions("PL_CONFIG_DEBUG")
            pl.add_static_link_libraries("dearimguid")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_definitions("PL_PLATFORM_WINDOWS")
                    pl.add_linker_flags("-incremental:no", "-nologo", "-noexp")
                    pl.add_static_link_libraries("ucrtd")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-W3", "-WX", "-Od", "-MDd", "-Zi", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_definitions("PL_PLATFORM_LINUX")
                    pl.add_compiler_flags("-fPIC", "-std=c++14", "--debug -g")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("PL_PLATFORM_APPLE")
                    pl.add_compiler_flags("-fPIC", "-ObjC++", "-std=c++14", "--debug -g", "-Wno-nullability-completeness")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

        with pl.configuration("debug"): add_debug_config()
        with pl.configuration("test"):
            pl.add_definitions("PL_CONFIG_TEST")
            add_debug_config()

        with pl.configuration("release"):

            pl.add_definitions("PL_CONFIG_RELEASE")
            pl.add_static_link_libraries("dearimgui")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_definitions("PL_PLATFORM_WINDOWS")
                    pl.add_linker_flags("-incremental:no", "-nologo", "-noexp")
                    pl.add_static_link_libraries("ucrt")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-W3", "-WX", "-O2", "-MD", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_definitions("PL_PLATFORM_LINUX")
                    pl.add_compiler_flags("-fPIC", "-std=c++14")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("PL_PLATFORM_APPLE")
                    pl.add_compiler_flags("-fPIC", "-ObjC++", "-std=c++14", "-Wno-nullability-completeness")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

    #-----------------------------------------------------------------------------
    # [SECTION] sandbox
    #-----------------------------------------------------------------------------

    with pl.target("sandbox", pl.TargetType.DYNAMIC_LIBRARY, reloadable=True):

        pl.add_source_files("../sandbox/app.cpp")
        pl.set_output_binary("app")

        # default config
        def add_debug_config():

            pl.add_definitions("PL_CONFIG_DEBUG")
            pl.add_static_link_libraries("dearimguid")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_definitions("PL_PLATFORM_WINDOWS")
                    pl.add_linker_flags("-incremental:no", "-nologo", "-noimplib", "-noexp")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-W3", "-WX", "-wd4201", "-wd4100",
                                          "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                          "-Od", "-MDd", "-Zi", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_definitions("PL_PLATFORM_LINUX")
                    pl.add_compiler_flags("-fPIC", "-std=c++14", "--debug -g")
                    pl.add_linker_flags("-ldl", "-lm", "-lstdc++")
                    
            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("PL_PLATFORM_APPLE")
                    pl.add_linker_flags("-lstdc++", "-ldl", "-lm")
                    pl.add_compiler_flags("-fPIC", "-ObjC++", "--debug", "-g", "-std=c++14")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

        with pl.configuration("debug"): add_debug_config()
        with pl.configuration("test"):
            pl.add_definitions("PL_CONFIG_TEST")
            add_debug_config()
        
        with pl.configuration("release"):

            pl.add_definitions("PL_CONFIG_RELEASE")
            pl.add_static_link_libraries("dearimgui")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_definitions("PL_PLATFORM_WINDOWS")
                    pl.add_linker_flags("-incremental:no", "-nologo", "-noimplib", "-noexp")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-W3", "-WX", "-wd4201", "-wd4100",
                                          "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                          "-O2", "-MD", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_definitions("PL_PLATFORM_LINUX")
                    pl.add_compiler_flags("-fPIC", "-std=c++14")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")
                    
            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("PL_PLATFORM_APPLE")
                    pl.add_linker_flags("-ldl", "-lm", "-lstdc++")
                    pl.add_compiler_flags("-fPIC", "-ObjC++", "-std=c++14")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

        with pl.configuration("moltenvk"):

            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("PL_PLATFORM_APPLE")
                    pl.add_static_link_libraries("dearimguid")
                    pl.add_link_directories("/usr/local/lib")
                    pl.add_linker_flags("-lstdc++", "-ldl", "-lm")
                    pl.add_compiler_flags("-fPIC", "-fmodules", "--debug", "-g", "-std=c++14")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

    pl.apply_profiles()
         
#-----------------------------------------------------------------------------
# [SECTION] generate scripts
#-----------------------------------------------------------------------------

# where to output build scripts
working_directory = os.path.dirname(os.path.abspath(__file__)) + "/../src"

if plat.system() == "Windows":
    win32.generate_build(working_directory + '/' + "build.bat")
elif plat.system() == "Darwin":
    apple.generate_build(working_directory + '/' + "build.sh")
elif plat.system() == "Linux":
    linux.generate_build(working_directory + '/' + "build.sh")

if len(sys.argv)  == 1:
    win32.generate_build(working_directory + '/' + "build_win32.bat")
    apple.generate_build(working_directory + '/' + "build_macos.sh")
    linux.generate_build(working_directory + '/' + "build_linux.sh")