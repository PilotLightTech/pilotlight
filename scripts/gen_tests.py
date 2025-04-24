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
    pl.set_hot_reload_target("../out/pilot_light_test")

    # project wide settings
    pl.set_output_directory("../out")
    pl.add_link_directories("../out")
    pl.add_include_directories("../examples", "../src", "../libs", "../extensions", "../out", "../dependencies/stb")

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

    # graphics backend
    pl.add_profile(configuration_filter=["debug", "release"], definitions=["PL_CPU_BACKEND"])
    
    # configs
    pl.add_profile(configuration_filter=["debug"], definitions=["_DEBUG", "PL_CONFIG_DEBUG"])
    pl.add_profile(configuration_filter=["release"], definitions=["NDEBUG", "PL_CONFIG_RELEASE"])

    with pl.target("pilot_light_test", pl.TargetType.EXECUTABLE):

        pl.add_source_files("main_lib_tests.c")
        pl.set_output_binary("pilot_light_test")

        with pl.configuration("debug"):

            pl.add_definitions("PL_CONFIG_DEBUG")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pass

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

        with pl.configuration("release"):

            pl.add_definitions("PL_CONFIG_RELEASE")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pass

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

    with pl.target("pilot_light", pl.TargetType.EXECUTABLE):

        pl.add_source_files("../src/pl_main_null.c")
        pl.set_output_binary("pilot_light")

        with pl.configuration("debug"):

            pl.add_definitions("PL_CONFIG_DEBUG")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("ucrtd", "user32", "Ole32")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-Wno-deprecated-declarations")

        with pl.configuration("release"):

            pl.add_definitions("PL_CONFIG_RELEASE")

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_static_link_libraries("ucrt", "user32", "Ole32")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")


            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-Wno-deprecated-declarations")

    #-----------------------------------------------------------------------------
    # [SECTION] extensions
    #-----------------------------------------------------------------------------

    extensions = [
        "pl_collision_ext",
        "pl_graphics_ext",
    ]

    for extension in extensions:

        with pl.target(extension, pl.TargetType.DYNAMIC_LIBRARY, False):

            pl.add_source_files("../extensions/" + extension + ".c")
            pl.set_output_binary(extension)
            
            # default config
            with pl.configuration("debug"):

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
    # [SECTION] app
    #-----------------------------------------------------------------------------

    with pl.target("tests", pl.TargetType.DYNAMIC_LIBRARY, False):

        pl.add_source_files("app_tests.c")
        pl.set_output_binary("tests")

        # default config
        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pass
            
            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

        # release
        with pl.configuration("release"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pass

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_dynamic_link_libraries("pthread")

            # mac os
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

        # vulkan on macos
        with pl.configuration("vulkan"):
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pass

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