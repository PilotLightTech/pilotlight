# gen_tests.py

# Index of this file:
# [SECTION] imports
# [SECTION] project
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
working_directory = os.path.dirname(os.path.abspath(__file__)) + "/../tests"

with pl.project("pilotlight_lib_tests"):
    
    # used to decide hot reloading
    pl.set_hot_reload_target("../out/pilot_light")
    pl.set_hot_reload_artifact_directory("../out-temp")

    # project wide settings
    pl.set_output_directory("../out")
    pl.add_include_directories("../examples", "../src", "../libs", "../extensions", "../out", "../dependencies/stb")
    pl.add_definitions("PL_CPU_BACKEND")

    #-----------------------------------------------------------------------------
    # [SECTION] lib c test ext
    #-----------------------------------------------------------------------------

    with pl.target("pilot_light_test_c", pl.TargetType.EXECUTABLE):

        pl.add_source_files("main_lib_tests.c")
        pl.set_output_binary("pilot_light_c")

        with pl.configuration("debug"):

            pl.add_definitions("PL_CONFIG_DEBUG", "_DEBUG")

            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-incremental:no")
                    pl.add_compiler_flags("-Od", "-MDd", "-Zi", "-Zc:preprocessor", "-nologo", "-std:c11", "-W4",
                                          "-WX", "-wd4201", "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105",
                                          "-wd4115", "-permissive-")
                    pl.add_include_directories('%WindowsSdkDir%Include\\um', '%WindowsSdkDir%Include\\shared')

            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-ldl", "-lm")
                    pl.add_compiler_flags("-std=gnu11", "-fPIC", "--debug", "-g")
                    pl.add_link_directories("/usr/lib/x86_64-linux-gnu")

            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("--debug", "-g", "-std=c99", "-fmodules", "-fPIC")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

        with pl.configuration("release"):

            pl.add_definitions("PL_CONFIG_RELEASE", "NDEBUG")

            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-incremental:no")
                    pl.add_compiler_flags("-O2", "-MD", "-Zc:preprocessor", "-nologo", "-std:c11", "-W4", "-WX",
                                          "-wd4201", "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                          "-permissive-")

            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-ldl", "-lm")
                    pl.add_compiler_flags("-std=gnu11", "-fPIC")
                    pl.add_link_directories("/usr/lib/x86_64-linux-gnu")

            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-std=c99", "-fmodules", "-fPIC")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

    #-----------------------------------------------------------------------------
    # [SECTION] lib c++ test ext
    #-----------------------------------------------------------------------------

    with pl.target("pilot_light_test_cpp", pl.TargetType.EXECUTABLE):

        pl.add_source_files("main_lib_tests.cpp")
        pl.set_output_binary("pilot_light_cpp")

        with pl.configuration("debug"):

            pl.add_definitions("PL_CONFIG_DEBUG", "_DEBUG")

            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-incremental:no")
                    pl.add_compiler_flags("-Od", "-MDd", "-Zi", "-Zc:preprocessor", "-nologo", "-std:c++14", "-W4",
                                          "-WX", "-wd4201", "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105",
                                          "-wd4115", "-permissive-")
                    pl.add_include_directories('%WindowsSdkDir%Include\\um', '%WindowsSdkDir%Include\\shared')

            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-ldl", "-lm", "-lstdc++")
                    pl.add_compiler_flags("-std=c++14", "-fPIC", "--debug", "-g")
                    pl.add_link_directories("/usr/lib/x86_64-linux-gnu")

            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("--debug", "-g", "-std=c++14", "-fmodules", "-fPIC")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")
                    pl.add_linker_flags("-ldl", "-lm", "-lstdc++")

        with pl.configuration("release"):

            pl.add_definitions("PL_CONFIG_RELEASE", "NDEBUG")

            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-incremental:no")
                    pl.add_compiler_flags("-O2", "-MD", "-Zc:preprocessor", "-nologo", "-std:c++14", "-W4", "-WX",
                                          "-wd4201", "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                          "-permissive-")

            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-ldl", "-lm", "-lstdc++")
                    pl.add_compiler_flags("-std=c++14", "-fPIC")
                    pl.add_link_directories("/usr/lib/x86_64-linux-gnu")

            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-std=c++14", "-fmodules", "-fPIC")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")
                    pl.add_linker_flags("-ldl", "-lm", "-lstdc++")

    #-----------------------------------------------------------------------------
    # [SECTION] pilot light null backend
    #-----------------------------------------------------------------------------

    with pl.target("pilot_light", pl.TargetType.EXECUTABLE):

        pl.add_source_files("../src/pl_main_null.c")
        pl.set_output_binary("pilot_light")

        with pl.configuration("debug"):

            pl.add_definitions("PL_CONFIG_DEBUG", "_DEBUG")

            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("ucrtd", "user32", "Ole32")
                    pl.add_linker_flags("-incremental:no")
                    pl.add_compiler_flags("-Od", "-MDd", "-Zi", "-Zc:preprocessor", "-nologo", "-std:c11", "-W4",
                                          "-WX", "-wd4201", "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105",
                                          "-wd4115", "-permissive-")

            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-ldl", "-lm")
                    pl.add_compiler_flags("-std=gnu11", "-fPIC", "--debug", "-g")
                    pl.add_link_directories("/usr/lib/x86_64-linux-gnu")

            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("--debug", "-g", "-Wno-deprecated-declarations", "-std=c99", "-fmodules", "-fPIC")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

        with pl.configuration("release"):

            pl.add_definitions("PL_CONFIG_RELEASE", "NDEBUG")

            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("ucrt", "user32", "Ole32")
                    pl.add_linker_flags("-incremental:no")
                    pl.add_compiler_flags("-O2", "-MD", "-Zc:preprocessor", "-nologo", "-std:c11", "-W4", "-WX",
                                          "-wd4201", "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                          "-permissive-")

            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-ldl", "-lm")
                    pl.add_compiler_flags("-std=gnu11", "-fPIC")
                    pl.add_link_directories("/usr/lib/x86_64-linux-gnu")

            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-Wno-deprecated-declarations", "-std=c99", "-fmodules", "-fPIC")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

    #-----------------------------------------------------------------------------
    # [SECTION] extensions
    #-----------------------------------------------------------------------------

    extensions = [
        "pl_collision_ext",
        "pl_graphics_ext",
        "pl_datetime_ext",
        "pl_compress_ext",
        "pl_pak_ext",
        "pl_vfs_ext",
        "pl_string_intern_ext",
    ]

    for extension in extensions:

        with pl.target(extension, pl.TargetType.DYNAMIC_LIBRARY, False):

            pl.add_source_files("../extensions/" + extension + ".c")
            pl.set_output_binary(extension)
            
            with pl.configuration("debug"):

                pl.add_definitions("PL_CONFIG_DEBUG", "_DEBUG")

                with pl.platform("Windows"):

                    with pl.compiler("msvc"):
                        pl.add_linker_flags("-incremental:no", "-noimplib", "-noexp")
                        pl.add_compiler_flags("-Od", "-MDd", "-Zi", "-Zc:preprocessor", "-nologo", "-std:c11", "-W4",
                                              "-WX", "-wd4201", "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105",
                                              "-wd4115", "-permissive-")

                with pl.platform("Linux"):
                    with pl.compiler("gcc"):
                        pl.add_linker_flags("-ldl", "-lm")
                        pl.add_compiler_flags("-std=gnu11", "-fPIC", "--debug", "-g")
                        pl.add_link_directories("/usr/lib/x86_64-linux-gnu")

                with pl.platform("Darwin"):
                    with pl.compiler("clang"):
                        pl.add_compiler_flags("--debug", "-g", "-std=c99", "-fmodules", "-fPIC")
                        pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

            with pl.configuration("release"):

                pl.add_definitions("PL_CONFIG_RELEASE", "NDEBUG")

                with pl.platform("Windows"):

                    with pl.compiler("msvc"):
                        pl.add_linker_flags("-incremental:no", "-noimplib", "-noexp")
                        pl.add_compiler_flags("-O2", "-MD", "-Zc:preprocessor", "-nologo", "-std:c11", "-W4", "-WX",
                                              "-wd4201", "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105",
                                              "-wd4115", "-permissive-")

                with pl.platform("Linux"):
                    with pl.compiler("gcc"):
                        pl.add_linker_flags("-ldl", "-lm")
                        pl.add_compiler_flags("-std=gnu11", "-fPIC")
                        pl.add_link_directories("/usr/lib/x86_64-linux-gnu")

                with pl.platform("Darwin"):
                    with pl.compiler("clang"):
                        pl.add_compiler_flags("-std=c99", "-fmodules", "-fPIC")
                        pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

    #-----------------------------------------------------------------------------
    # [SECTION] platform extension
    #-----------------------------------------------------------------------------

    with pl.target("pl_platform_ext", pl.TargetType.DYNAMIC_LIBRARY, False):
    
        pl.set_output_binary("pl_platform_ext")

        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_source_files("../extensions/pl_platform_win32_ext.c")
                    pl.add_static_link_libraries("ucrtd", "user32", "Ole32")
                    pl.add_compiler_flags("-std:c11", "-Od", "-MDd", "-Zi", "-Zc:preprocessor", "-nologo", "-W4", "-WX", "-wd4201",
                                "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115", "-permissive-")
                    pl.add_linker_flags("-incremental:no", "-noimplib", "-noexp")
                        
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("../extensions/pl_platform_linux_ext.c")
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_compiler_flags("-std=gnu11", "-fPIC", "--debug", "-g")
                    pl.add_linker_flags("-ldl", "-lm")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../extensions/pl_platform_macos_ext.m")
                    pl.add_compiler_flags("-std=c99", "-fmodules", "-ObjC", "-fPIC", "--debug", "-g")

        with pl.configuration("release"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_source_files("../extensions/pl_platform_win32_ext.c")
                    pl.add_static_link_libraries("ucrt", "user32", "Ole32")
                    pl.add_compiler_flags("-std:c11", "-O2", "-MD", "-Zc:preprocessor", "-nologo", "-W4", "-WX", "-wd4201",
                                "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115", "-permissive-")
                    pl.add_linker_flags("-incremental:no", "-noimplib", "-noexp")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_source_files("../extensions/pl_platform_linux_ext.c")
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_compiler_flags("-std=gnu11", "-fPIC")
                    pl.add_linker_flags("-ldl", "-lm")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_source_files("../extensions/pl_platform_macos_ext.m")
                    pl.add_compiler_flags("-std=c99", "-fmodules", "-ObjC", "-fPIC")

    #-----------------------------------------------------------------------------
    # [SECTION] c app
    #-----------------------------------------------------------------------------

    with pl.target("tests_c", pl.TargetType.DYNAMIC_LIBRARY, False):

        pl.set_output_binary("tests_c")
        pl.add_source_files("app_tests.c")

        with pl.configuration("debug"):

            pl.add_definitions("PL_CONFIG_DEBUG", "_DEBUG")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-incremental:no", "-noimplib", "-noexp")
                    pl.add_compiler_flags("-Od", "-MDd", "-Zi", "-Zc:preprocessor", "-nologo", "-std:c11", "-W4",
                                          "-WX", "-wd4201", "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105",
                                          "-wd4115", "-permissive-")
            
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-ldl", "-lm")
                    pl.add_compiler_flags("-std=gnu11", "-fPIC", "--debug", "-g")
                    pl.add_link_directories("/usr/lib/x86_64-linux-gnu")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("--debug", "-g", "-std=c99", "-fmodules", "-fPIC")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

        # release
        with pl.configuration("release"):

            pl.add_definitions("PL_CONFIG_RELEASE", "NDEBUG")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-incremental:no", "-noimplib", "-noexp")
                    pl.add_compiler_flags("-O2", "-MD", "-Zc:preprocessor", "-nologo", "-std:c11", "-W4", "-WX",
                                          "-wd4201", "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                          "-permissive-")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-ldl", "-lm")
                    pl.add_compiler_flags("-std=gnu11", "-fPIC")
                    pl.add_link_directories("/usr/lib/x86_64-linux-gnu")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-std=c99", "-fmodules", "-fPIC")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

    #-----------------------------------------------------------------------------
    # [SECTION] c++ app
    #-----------------------------------------------------------------------------

    with pl.target("tests_cpp", pl.TargetType.DYNAMIC_LIBRARY, False):

        pl.set_output_binary("tests_cpp")
        pl.add_source_files("app_tests.cpp")

        with pl.configuration("debug"):

            pl.add_definitions("PL_CONFIG_DEBUG", "_DEBUG")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-incremental:no", "-noimplib", "-noexp")
                    pl.add_compiler_flags("-Od", "-MDd", "-Zi", "-Zc:preprocessor", "-nologo", "-std:c++14", "-W4",
                                          "-WX", "-wd4201", "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105",
                                          "-wd4115", "-permissive-")
            
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-ldl", "-lm", "-lstdc++")
                    pl.add_compiler_flags("-std=gnu11", "-fPIC", "--debug", "-g")
                    pl.add_link_directories("/usr/lib/x86_64-linux-gnu")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("--debug", "-g", "-std=c++14", "-fmodules", "-fPIC")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")

        # release
        with pl.configuration("release"):

            pl.add_definitions("PL_CONFIG_RELEASE", "NDEBUG")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_linker_flags("-incremental:no", "-noimplib", "-noexp")
                    pl.add_compiler_flags("-O2", "-MD", "-Zc:preprocessor", "-nologo", "-std:c++14", "-W4", "-WX",
                                          "-wd4201", "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                          "-permissive-")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_linker_flags("-ldl", "-lm", "-lstdc++")
                    pl.add_compiler_flags("-std=c++14", "-fPIC")
                    pl.add_link_directories("/usr/lib/x86_64-linux-gnu")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-std=c++14", "-fmodules", "-fPIC")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")
                    pl.add_linker_flags("-ldl", "-lm", "-lstdc++")

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