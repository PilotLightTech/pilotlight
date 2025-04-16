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
    pl.add_hot_reload_target("../out/pilot_light")

    # project wide settings
    pl.set_output_directory("../out")
    pl.add_link_directories("../out")
    pl.add_include_directories("../editor", "../src", "../libs", "../extensions", "../out", "../dependencies/stb",
                               "../dependencies/cgltf", "../dependencies/imgui", '../dependencies/glfw/include')
    pl.add_definitions("PL_UNITY_BUILD")

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
                    compiler_flags=["-Zc:preprocessor", "-nologo", "-W4", "-WX", "-wd4201",
                                "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115", "-permissive-"])
    pl.add_profile(compiler_filter=["msvc"],
                    configuration_filter=["debug", "debug_experimental"],
                    compiler_flags=["-Od", "-MDd", "-Zi"])
    pl.add_profile(compiler_filter=["msvc"],
                    configuration_filter=["release", "release_experimental"],
                    compiler_flags=["-O2", "-MD"])


    # linux or gcc only
    pl.add_profile(platform_filter=["Linux"],
                    link_directories=["/usr/lib/x86_64-linux-gnu"])
    pl.add_profile(compiler_filter=["gcc"],
                    linker_flags=["-ldl", "-lm"],
                    compiler_flags=["-std=gnu11", "-fPIC"])
    pl.add_profile(compiler_filter=["gcc"],
                    configuration_filter=["debug", "debug_experimental"],
                    compiler_flags=["--debug", "-g"])

    # macos or clang only
    pl.add_profile(platform_filter=["Darwin"],
                    link_frameworks=["Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore"])
    pl.add_profile(compiler_filter=["clang"],
                    linker_flags=["-Wl,-rpath,/usr/local/lib"],
                    compiler_flags=["-std=c99", "-fmodules", "-ObjC", "-fPIC"])
    pl.add_profile(compiler_filter=["clang"],
                    configuration_filter=["debug", "moltenvk", "debug_experimental", "moltenvk_experimental"],
                    compiler_flags=["--debug", "-g"])

    # graphics backends
    pl.add_profile(configuration_filter=["debug", "release", "debug_experimental", "release_experimental"],
                   compiler_filter=["gcc", "msvc"],
                    definitions=["PL_VULKAN_BACKEND"])
    pl.add_profile(configuration_filter=["debug", "release", "debug_experimental", "release_experimental"], platform_filter=["Darwin"],
                    definitions=["PL_METAL_BACKEND"])
    pl.add_profile(configuration_filter=["moltenvk", "moltenvk_experimental"],
                    definitions=["PL_VULKAN_BACKEND"])
    pl.add_profile(configuration_filter=["debug", "release", "debug_experimental", "release_experimental"],
                    definitions=["PL_UNITY_BUILD"])
    
    # configs
    pl.add_profile(configuration_filter=["debug", "moltenvk", "debug_experimental", "moltenvk_experimental"], definitions=["_DEBUG", "PL_CONFIG_DEBUG"])
    pl.add_profile(configuration_filter=["release", "release_experimental"], definitions=["NDEBUG", "PL_CONFIG_RELEASE"])
                    
    #-----------------------------------------------------------------------------
    # [SECTION] extensions
    #-----------------------------------------------------------------------------

    with pl.target("pl_unity_ext", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.add_source_files("../extensions/pl_unity_ext.c")
        pl.set_output_binary("pl_unity_ext")
        pl.add_definitions("PL_INCLUDE_SPIRV_CROSS")

        def add_debug_unity_ext():

            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c-shared", "vulkan-1")
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_linker_flags("-nodefaultlib:MSVCRT")
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared", "xcb", "X11", "X11-xcb",
                                                    "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread",
                                                    "vulkan")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")

        def add_release_unity_ext():
            
            # win32
            with pl.platform("Windows"):

                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("shaderc_combined", "spirv-cross-c-shared", "vulkan-1")
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared", "xcb", "X11", "X11-xcb",
                                                    "xkbcommon", "xcb-cursor", "xcb-xfixes", "xcb-keysyms", "pthread",
                                                    "vulkan")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared")
                    pl.add_compiler_flags("-Wno-deprecated-declarations")

        with pl.configuration("debug"):                add_debug_unity_ext()
        with pl.configuration("release"):              add_release_unity_ext()
        with pl.configuration("debug_experimental"):   add_debug_unity_ext()
        with pl.configuration("release_experimental"): add_release_unity_ext()

        # vulkan on macos
        with pl.configuration("moltenvk"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared", "pthread", "vulkan")

        with pl.configuration("moltenvk_experimental"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared", "pthread", "vulkan")
                    
    #-----------------------------------------------------------------------------
    # [SECTION] ecs scripts
    #-----------------------------------------------------------------------------

    with pl.target("pl_script_camera", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.set_output_binary("pl_script_camera")
        pl.add_source_files("../extensions/pl_script_camera.c")

        def add_script_ext():

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_compiler_flags("-std:c11")
                                
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pass

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

        with pl.configuration("debug"):                add_script_ext()
        with pl.configuration("release"):              add_script_ext()
        with pl.configuration("debug_experimental"):   add_script_ext()
        with pl.configuration("release_experimental"): add_script_ext()

        # vulkan on macos
        with pl.configuration("moltenvk"):

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_dynamic_link_libraries("shaderc_shared")

        with pl.configuration("moltenvk_experimental"):

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_dynamic_link_libraries("shaderc_shared")

    #-----------------------------------------------------------------------------
    # [SECTION] platform extension
    #-----------------------------------------------------------------------------

    with pl.target("pl_platform_ext", pl.TargetType.DYNAMIC_LIBRARY, False):
    
        pl.set_output_binary("pl_platform_ext")

        def add_platform_ext_debug():

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_source_files("../extensions/pl_platform_win32_ext.c")
                    pl.add_static_link_libraries("ucrtd", "user32", "Ole32")
                    pl.add_compiler_flags("-std:c11")
                        
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("../extensions/pl_platform_linux_ext.c")
                    pl.add_dynamic_link_libraries("pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../extensions/pl_platform_macos_ext.m")

        def add_platform_ext_release():

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_source_files("../extensions/pl_platform_win32_ext.c")
                    pl.add_static_link_libraries("ucrt", "user32", "Ole32")
                    pl.add_compiler_flags("-std:c11")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("../extensions/pl_platform_linux_ext.c")
                    pl.add_dynamic_link_libraries("pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../extensions/pl_platform_macos_ext.m")

        with pl.configuration("debug"):                add_platform_ext_debug()
        with pl.configuration("release"):              add_platform_ext_release()
        with pl.configuration("debug_experimental"):   add_platform_ext_debug()
        with pl.configuration("release_experimental"): add_platform_ext_release()

        # vulkan on macos
        with pl.configuration("moltenvk"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../extensions/pl_platform_macos_ext.m")

        with pl.configuration("moltenvk_experimental"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../extensions/pl_platform_macos_ext.m")

    #-----------------------------------------------------------------------------
    # [SECTION] app
    #-----------------------------------------------------------------------------

    with pl.target("app", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.add_source_files("../editor/app.c")
        pl.set_output_binary("app")

        def add_app():
            
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pass
            
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes",
                                                    "xcb-keysyms", "pthread")
                    
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

        with pl.configuration("debug"):                add_app()
        with pl.configuration("release"):              add_app()
        with pl.configuration("debug_experimental"):   add_app()
        with pl.configuration("release_experimental"): add_app()

        # vulkan on macos
        with pl.configuration("moltenvk"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

        with pl.configuration("moltenvk_experimental"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

    #-----------------------------------------------------------------------------
    # [SECTION] pilot_light
    #-----------------------------------------------------------------------------

    with pl.target("pilot_light", pl.TargetType.EXECUTABLE, False):
    
        pl.set_output_binary("pilot_light")
    
        # default config
        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("ucrtd", "user32", "Ole32")
                    pl.add_source_files("pl_main_win32.c")
                    pl.add_compiler_flags("-std:c11")
                    
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("pl_main_x11.c")
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes",
                                                  "xcb-keysyms", "pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("pl_main_macos.m")
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
                    pl.add_source_files("pl_main_x11.c")
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "xcb-cursor", "xcb-xfixes",
                                                  "xcb-keysyms", "pthread")

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

    #-----------------------------------------------------------------------------
    # [SECTION] experimental editor section
    #-----------------------------------------------------------------------------

    # remove all the profiles so settings are explicit
    pl.stash_profiles()

    #-----------------------------------------------------------------------------
    # [SECTION] glfw
    #-----------------------------------------------------------------------------

    with pl.target("glfw", pl.TargetType.STATIC_LIBRARY, False, False):

        pl.add_source_files("../dependencies/glfw/src/glfw_unity.c")
        pl.add_source_files("../dependencies/glfw/src/null_window.c")

        with pl.configuration("debug_experimental"):

            pl.set_output_binary("glfwd")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_definitions("UNICODE", "_UNICODE", "_CRT_SECURE_NO_WARNINGS", "_GLFW_VULKAN_STATIC", "_GLFW_WIN32", "_DEBUG")
                    pl.add_compiler_flags("-nologo", "-std:c11", "-W3", "-wd5105", "-Od", "-MDd", "-Zi", "-permissive")
                    pl.add_linker_flags("-incremental:no", "-nologo")
            
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_definitions("_GLFW_VULKAN_STATIC", "_GLFW_X11", "_DEBUG")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan', '/usr/include/vulkan')
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "pthread", "xcb-cursor", "vulkan")
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_compiler_flags("-fPIC", "-std=gnu99", "--debug -g")
                    pl.add_linker_flags("-ldl -lm")
                    pl.add_source_files("../dependencies/glfw/src/posix_poll.c")

            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("_GLFW_VULKAN_STATIC", "_GLFW_COCOA", "_DEBUG")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan', '/usr/include/vulkan')
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared", "vulkan")
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_compiler_flags("-Wno-deprecated-declarations", "--debug -g", "-std=c99", "-fmodules", "-ObjC", "-fPIC")
                    pl.add_linker_flags("-Wl,-rpath,/usr/local/lib")
                    pl.add_link_frameworks("Cocoa", "IOKit", "CoreFoundation")

        with pl.configuration("release_experimental"):

            pl.set_output_binary("glfw")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_definitions("UNICODE", "_UNICODE", "_CRT_SECURE_NO_WARNINGS", "_GLFW_VULKAN_STATIC", "_GLFW_WIN32")
                    pl.add_compiler_flags("-nologo", "-std:c11", "-W3", "-wd5105", "-O2", "-MD", "-Zi", "-permissive")
                    pl.add_linker_flags("-incremental:no", "-nologo")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_definitions("_GLFW_VULKAN_STATIC", "_GLFW_X11")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan', '/usr/include/vulkan')
                    pl.add_dynamic_link_libraries("xcb", "X11", "X11-xcb", "xkbcommon", "pthread", "xcb-cursor", "vulkan")
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_compiler_flags("-fPIC", "-std=gnu99")
                    pl.add_linker_flags("-ldl -lm")
                    pl.add_source_files("../dependencies/glfw/src/posix_poll.c")

            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("_GLFW_VULKAN_STATIC", "_GLFW_COCOA")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan', '/usr/include/vulkan')
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared", "vulkan")
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_compiler_flags("-std=c99", "-fmodules", "-ObjC", "-fPIC", "-Wno-deprecated-declarations")
                    pl.add_linker_flags("-Wl,-rpath,/usr/local/lib")
                    pl.add_link_frameworks("Cocoa", "IOKit", "CoreFoundation")

        with pl.configuration("moltenvk_experimental"):

            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.set_output_binary("glfwd")
                    pl.add_definitions("_GLFW_VULKAN_STATIC", "_GLFW_COCOA", "_DEBUG")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan', '/usr/include/vulkan')
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared", "vulkan")
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_compiler_flags("-Wno-deprecated-declarations", "--debug -g", "-std=c99", "-fmodules", "-ObjC", "-fPIC")
                    pl.add_linker_flags("-Wl,-rpath,/usr/local/lib")
                    pl.add_link_frameworks("Cocoa", "IOKit", "CoreFoundation")

    #-----------------------------------------------------------------------------
    # [SECTION] imgui & implot
    #-----------------------------------------------------------------------------

    with pl.target("imgui", pl.TargetType.STATIC_LIBRARY, False, False):

        # imgui & imgui
        pl.add_source_files("../dependencies/imgui/imgui_unity.cpp")

        # default config
        with pl.configuration("debug_experimental"):

            pl.set_output_binary("dearimguid")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-incremental:no", "-nologo")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-WX", "-Od", "-MDd", "-Zi", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_compiler_flags("-fPIC", "-std=c++14", "--debug -g")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-fPIC", "-std=c++14", "--debug -g")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

        with pl.configuration("release_experimental"):

            pl.set_output_binary("dearimgui")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-incremental:no", "-nologo")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-WX", "-O2", "-MD", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_compiler_flags("-fPIC", "-std=c++14")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-fPIC", "-std=c++14")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

        with pl.configuration("moltenvk_experimental"):

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.set_output_binary("dearimguid")
                    pl.add_compiler_flags("-fPIC", "-std=c++14", "--debug -g")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

    #-----------------------------------------------------------------------------
    # [SECTION] editor
    #-----------------------------------------------------------------------------

    with pl.target("editor", pl.TargetType.DYNAMIC_LIBRARY, True):

        pl.add_source_files("../editor/editor.cpp")
        pl.set_output_binary("editor")
        pl.add_definitions("PL_EXPERIMENTAL")

        # default config
        with pl.configuration("debug_experimental"):

            pl.add_static_link_libraries("dearimguid")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-incremental:no", "-nologo", "-noimplib", "-noexp")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-W3", "-WX", "-wd4201", "-wd4100",
                                          "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                          "-Od", "-MDd", "-Zi", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_compiler_flags("-fPIC", "-std=c++14", "--debug -g")
                    pl.add_linker_flags("-ldl", "-lm", "-lstdc++")
                    
            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_linker_flags("-lstdc++", "-ldl", "-lm")
                    pl.add_compiler_flags("-fPIC", "-ObjC++", "--debug", "-g", "-std=c++14")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

        with pl.configuration("release_experimental"):

            pl.add_static_link_libraries("dearimgui")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-incremental:no", "-nologo", "-noimplib", "-noexp")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-W3", "-WX", "-wd4201", "-wd4100",
                                          "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                          "-O2", "-MD", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_compiler_flags("-fPIC", "-std=c++14")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")
                    
            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_linker_flags("-ldl", "-lm", "-lstdc++")
                    pl.add_compiler_flags("-fPIC", "-ObjC++", "-std=c++14")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

        with pl.configuration("moltenvk_experimental"):

            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_static_link_libraries("dearimguid")
                    pl.add_linker_flags("-lstdc++", "-ldl", "-lm", "-Wl,-rpath,/usr/local/lib")
                    pl.add_compiler_flags("-fPIC", "-fmodules", "--debug", "-g", "-std=c++14")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")
            
    #-----------------------------------------------------------------------------
    # [SECTION] pl_dear_imgui_ext
    #-----------------------------------------------------------------------------

    with pl.target("pl_dear_imgui_ext", pl.TargetType.DYNAMIC_LIBRARY, False, True):

        pl.add_source_files("../editor/pl_dear_imgui_ext.cpp")
        pl.set_output_binary("pl_dear_imgui_ext")
        pl.add_definitions("PL_EXPERIMENTAL")

        with pl.configuration("debug_experimental"):

            pl.add_static_link_libraries("glfwd", "dearimguid")

            # win32
            with pl.platform("Windows"):
                pl.add_definitions("PL_VULKAN_BACKEND")
                with pl.compiler("msvc"):
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_linker_flags("-incremental:no", "-nologo", "-noimplib", "-noexp")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_static_link_libraries("ucrtd", "vulkan-1")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-W3", "-WX", "-Od", "-MDd", "-Zi", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_definitions("PL_VULKAN_BACKEND")
                    pl.add_compiler_flags("-fPIC", "-std=c++14", "--debug -g")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_dynamic_link_libraries("vulkan")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("PL_METAL_BACKEND")
                    pl.add_compiler_flags("-fPIC", "-ObjC++", "-std=c++14", "--debug -g", "-Wno-nullability-completeness")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

        with pl.configuration("release_experimental"):

            pl.add_static_link_libraries("glfw", "dearimgui")

            # win32
            with pl.platform("Windows"):
                pl.add_definitions("PL_VULKAN_BACKEND")
                with pl.compiler("msvc"):
                    pl.add_include_directories("%VULKAN_SDK%\\Include")
                    pl.add_linker_flags("-incremental:no", "-nologo", "-noimplib", "-noexp")
                    pl.add_link_directories('%VULKAN_SDK%\\Lib')
                    pl.add_static_link_libraries("ucrtd", "vulkan-1")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-W3", "-WX", "-O2", "-MD", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_definitions("PL_VULKAN_BACKEND")
                    pl.add_compiler_flags("-fPIC", "-std=c++14")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_dynamic_link_libraries("vulkan")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("PL_METAL_BACKEND")
                    pl.add_compiler_flags("-fPIC", "-ObjC++", "-std=c++14", "-Wno-nullability-completeness")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

        with pl.configuration("moltenvk_experimental"):

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_static_link_libraries("glfwd", "dearimguid")
                    pl.add_definitions("PL_VULKAN_BACKEND")
                    pl.add_compiler_flags("-fPIC", "-std=c++14", "--debug -g")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")
                    pl.add_dynamic_link_libraries("spirv-cross-c-shared", "shaderc_shared", "pthread", "vulkan")

    #-----------------------------------------------------------------------------
    # [SECTION] pilot light glfw backend
    #-----------------------------------------------------------------------------

    with pl.target("pilot_light_experimental", pl.TargetType.EXECUTABLE, False, True):
    
        pl.add_source_files("../editor/pl_main_glfw.cpp")
        pl.set_output_binary("pilot_light")
        pl.add_definitions("PL_EXPERIMENTAL")

        # default config
        with pl.configuration("debug_experimental"):

            pl.add_static_link_libraries("glfwd", "dearimguid")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_definitions("PL_VULKAN_BACKEND")
                    pl.add_linker_flags("-incremental:no", "-nologo")
                    pl.add_static_link_libraries("user32", "Shell32", "Ole32", "gdi32", "ucrtd")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-W3", "-WX", "-wd4996", "-Od", "-MDd", "-Zi", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_definitions("PL_VULKAN_BACKEND")
                    pl.add_compiler_flags("-fPIC", "-std=c++14", "--debug -g")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_dynamic_link_libraries("vulkan", "pthread")
                    
            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("PL_METAL_BACKEND")
                    pl.add_compiler_flags("-fPIC", "-ObjC++", "--debug -g", "-std=c++11")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

        with pl.configuration("release_experimental"):

            pl.add_static_link_libraries("glfw", "dearimgui")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_definitions("PL_VULKAN_BACKEND")
                    pl.add_linker_flags("-incremental:no", "-nologo")
                    pl.add_static_link_libraries("user32", "Shell32", "Ole32", "gdi32", "ucrtd")
                    pl.add_compiler_flags("-nologo", "-std:c++14", "-W3", "-WX", "-wd4996", "-O2", "-MD", "-permissive")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_definitions("PL_VULKAN_BACKEND")
                    pl.add_compiler_flags("-fPIC", "-std=c++14")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")
                    pl.add_include_directories('$VULKAN_SDK/include', '/usr/include/vulkan')
                    pl.add_link_directories('$VULKAN_SDK/lib')
                    pl.add_dynamic_link_libraries("vulkan", "pthread")
                    
            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("PL_METAL_BACKEND")
                    pl.add_compiler_flags("-fPIC", "-ObjC++", "-std=c++11")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++")

        with pl.configuration("moltenvk_experimental"):

            # apple
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_definitions("PL_VULKAN_BACKEND")
                    pl.add_compiler_flags("--debug -g", "-std=c++11", "-fPIC", "-ObjC++", "-fmodules")
                    pl.add_linker_flags("-ldl -lm", "-lstdc++", "-Wl,-rpath,/usr/local/lib")
                    pl.add_dynamic_link_libraries("vulkan", "pthread")
                    pl.add_static_link_libraries("glfwd", "dearimguid")

    pl.apply_profiles()
         
#-----------------------------------------------------------------------------
# [SECTION] generate scripts
#-----------------------------------------------------------------------------

if plat.system() == "Windows":
    win32.generate_build(working_directory + '/' + "build.bat")
elif plat.system() == "Darwin":
    apple.generate_build(working_directory + '/' + "build.sh")
elif plat.system() == "Linux":
    linux.generate_build(working_directory + '/' + "build.sh")

win32.generate_build(working_directory + '/' + "build_win32.bat")
apple.generate_build(working_directory + '/' + "build_macos.sh")
linux.generate_build(working_directory + '/' + "build_linux.sh")