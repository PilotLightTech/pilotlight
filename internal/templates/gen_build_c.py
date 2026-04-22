# gen_dev.py

# Index of this file:
# [SECTION] imports
# [SECTION] project
# [SECTION] profiles
# [SECTION] extensions
# [SECTION] ecs scripts
# [SECTION] platform extension
# [SECTION] app
# [SECTION] pilot_light
# [SECTION] experimental editor section (temporary)
# [SECTION] glfw
# [SECTION] imgui & implot
# [SECTION] editor app
# [SECTION] pl_dear_imgui_ext
# [SECTION] pilot light glfw backend
# [SECTION] generate_scripts

#-----------------------------------------------------------------------------
# [SECTION] imports
#-----------------------------------------------------------------------------

import os
import sys
import platform as plat

import pl_build.core as pl
import pl_build.backend_win32 as win32
import pl_build.backend_linux as linux
import pl_build.backend_macos as apple

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
    pl.add_include_directories(output_directory,
                               "../src",
                               "../shaders",
                               "../dependencies/pilotlight/shaders",
                               "../dependencies/pilotlight/include",
                               "../dependencies/stb",
                               "../dependencies/pilotlight/src",
                               "../dependencies/cgltf")
    pl.add_definitions("PL_UNITY_BUILD")

    #-----------------------------------------------------------------------------
    # [SECTION] profiles
    #-----------------------------------------------------------------------------

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
    pl.add_profile(configuration_filter=["debug", "release"],
                   compiler_filter=["gcc", "msvc"],
                    definitions=["PL_VULKAN_BACKEND"])
    pl.add_profile(configuration_filter=["debug", "release"], platform_filter=["Darwin"],
                    definitions=["PL_METAL_BACKEND"])
    pl.add_profile(configuration_filter=["debug", "release"],
                    definitions=["PL_UNITY_BUILD"])
    
    # configs
    pl.add_profile(configuration_filter=["debug"], definitions=["_DEBUG", "PL_CONFIG_DEBUG"])
    pl.add_profile(configuration_filter=["release"], definitions=["NDEBUG", "PL_CONFIG_RELEASE"])

    #-----------------------------------------------------------------------------
    # [SECTION] pilot_light
    #-----------------------------------------------------------------------------

    with pl.target("pilot_light", pl.TargetType.EXECUTABLE, False, True):
    
        pl.set_output_binary("pilot_light")
    
        # default config
        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("ucrtd", "user32", "Ole32")
                    pl.add_source_files("../dependencies/pilotlight/src/pl_main_win32.c")
                    pl.add_compiler_flags("-std:c11")
                    
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("../dependencies/pilotlight/src/pl_main_x11.c")
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes",
                                                  "xcb-keysyms", "pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../dependencies/pilotlight/src/pl_main_macos.m")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
        
        # release
        with pl.configuration("release"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("ucrt", "user32", "Ole32")
                    pl.add_source_files("../dependencies/pilotlight/src/pl_main_win32.c")
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("../dependencies/pilotlight/src/pl_main_x11.c")
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes",
                                                  "xcb-keysyms", "pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../dependencies/pilotlight/src/pl_main_macos.m")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")

    #-----------------------------------------------------------------------------
    # [SECTION] extensions
    #-----------------------------------------------------------------------------

    with pl.target("pl_unity_ext", pl.TargetType.DYNAMIC_LIBRARY, False, False):

        pl.add_source_files("../dependencies/pilotlight/src/pl_unity_ext.c")
        pl.set_output_binary("pl_unity_ext")

        with pl.configuration("debug"): 

            # win32
            with pl.platform("Windows"):

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
                with pl.compiler("clang"):
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib', "/usr/local/lib")

        with pl.configuration("release"): 
            
            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("vulkan-1")
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
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
                with pl.compiler("clang"):
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c", "spirv-cross-core", "spirv-cross-cpp",
                        "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl", "spirv-cross-reflect", "spirv-cross-util")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")
                    pl.add_linker_flags("-lstdc++")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib', "/usr/local/lib")
                    
    #-----------------------------------------------------------------------------
    # [SECTION] ecs scripts
    #-----------------------------------------------------------------------------

    with pl.target("pl_script_camera", pl.TargetType.DYNAMIC_LIBRARY, False, False):

        pl.set_output_binary("pl_script_camera")
        pl.add_source_files("../dependencies/pilotlight/src/pl_script_camera.c")

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

        with pl.configuration("debug"):   add_script_ext()
        with pl.configuration("release"): add_script_ext()

    #-----------------------------------------------------------------------------
    # [SECTION] platform extension
    #-----------------------------------------------------------------------------

    with pl.target("pl_platform_ext", pl.TargetType.DYNAMIC_LIBRARY, False, False):
    
        pl.set_output_binary("pl_platform_ext")

        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_source_files("../dependencies/pilotlight/src/pl_platform_win32_ext.c")
                    pl.add_static_link_libraries("ucrtd", "user32", "Ole32")
                    pl.add_compiler_flags("-std:c11")
                        
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("../dependencies/pilotlight/src/pl_platform_linux_ext.c")
                    pl.add_dynamic_link_libraries("pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../dependencies/pilotlight/src/pl_platform_macos_ext.m")

        with pl.configuration("release"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_source_files("../dependencies/pilotlight/src/pl_platform_win32_ext.c")
                    pl.add_static_link_libraries("ucrt", "user32", "Ole32")
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("../dependencies/pilotlight/src/pl_platform_linux_ext.c")
                    pl.add_dynamic_link_libraries("pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../dependencies/pilotlight/src/pl_platform_macos_ext.m")

    #-----------------------------------------------------------------------------
    # [SECTION] app
    #-----------------------------------------------------------------------------

    with pl.target("app", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.add_source_files("../src/app.c")
        pl.set_output_binary("app")
        pl.add_dynamic_link_libraries("pl_unity_ext", "pl_platform_ext")

        def add_app():
            
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-noimplib")
            
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes",
                                                    "xcb-keysyms", "pthread")
                    
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

        with pl.configuration("debug"):   add_app()
        with pl.configuration("release"): add_app()
  
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