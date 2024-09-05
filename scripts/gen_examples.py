###############################################################################
#                              file index                                     #
###############################################################################
#                               imports                                       #
#                               project                                       #
#                               examples                                      #
#                           generate scripts                                  #
###############################################################################

###############################################################################
#                               imports                                       #
###############################################################################

import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/../pl_build")

import pl_build as pl
import pl_build_win32 as win32
import pl_build_linux as linux
import pl_build_macos as apple

###############################################################################
#                               project                                       #
###############################################################################

with pl.project("pilotlight"):
    
    # configurations
    pl.add_configuration("debug")

    # where to output build scripts
    pl.set_working_directory(os.path.dirname(os.path.abspath(__file__)) + "/../examples")

    # used to decide hot reloading
    pl.set_hot_reload_target("pilot_light")

    # project wide settings
    pl.set_output_directory("../out")
    pl.add_link_directories("../out")
    pl.add_definitions("_USE_MATH_DEFINES", "PL_PROFILING_ON", "PL_ALLOW_HOT_RELOAD", "PL_ENABLE_VALIDATION_LAYERS")
    pl.add_include_directories("../examples", "../src", "../libs", "../extensions", "../out", "../dependencies/stb")
        
    ###############################################################################
    #                                  examples                                   #
    ###############################################################################

    examples = [
        'example_0',
        'example_1',
        'example_2',
        'example_3',
        'example_4',
        'example_5',
        'example_6',
        'example_8',
    ]

    for name in examples:

        with pl.target(name, pl.TargetType.DYNAMIC_LIBRARY, True):

            pl.add_source_files(name + ".c")
            pl.set_output_binary(name)

            with pl.configuration("debug"):

                # win32
                with pl.platform("Windows"):
                    with pl.compiler("msvc"):
                        pl.add_static_link_libraries("pilotlight")
                        pl.add_compiler_flags("-Zc:preprocessor", "-nologo", "-std:c11", "-W4", "-WX", "-wd4201",
                                              "-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115",
                                              "-permissive-", "-Od", "-MDd", "-Zi")
                        
                # linux
                with pl.platform("Linux"):
                    with pl.compiler("gcc"):
                        pl.add_static_link_libraries("pilotlight")
                        pl.add_link_directories("/usr/lib/x86_64-linux-gnu")
                        pl.add_compiler_flags("-std=gnu11", "-fPIC", "--debug", "-g")
                        pl.add_linker_flags("-ldl", "-lm")
                
                # macos
                with pl.platform("Darwin"):
                    with pl.compiler("clang"):
                        pl.add_static_link_libraries("pilotlight")
                        pl.add_compiler_flags("-std=c99", "--debug", "-g", "-fmodules", "-ObjC", "-fPIC")
                        pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")
                        pl.add_linker_flags("-Wl,-rpath,/usr/local/lib")

###############################################################################
#                           generate scripts                                  #
###############################################################################

win32.generate_build("build_win32.bat", "Windows", "msvc", {"dev env setup" : True})
linux.generate_build("build_linux.sh", "Linux", "gcc", None)
apple.generate_build("build_macos.sh", "Darwin", "clang", None)