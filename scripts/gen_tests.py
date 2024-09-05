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

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/..")

import pl_build.core as pl
import pl_build.backend_win32 as win32
import pl_build.backend_linux as linux
import pl_build.backend_macos as apple

#-----------------------------------------------------------------------------
# [SECTION] project
#-----------------------------------------------------------------------------

# where to output build scripts
working_directory = os.path.dirname(os.path.abspath(__file__)) + "/../tests"

with pl.project("pilotlight"):
    
    # used to decide hot reloading
    pl.set_hot_reload_target("pilot_light_test")

    # project wide settings
    pl.set_output_directory("../out")
    pl.add_link_directories("../out")
    pl.add_include_directories("../examples", "../src", "../libs", "../extensions", "../out", "../dependencies/stb")
        
    with pl.target("pilot_light_test", pl.TargetType.EXECUTABLE):

        pl.add_source_files("main_tests.c")
        pl.set_output_binary("pilot_light_test")

        with pl.configuration("debug"):

            # win32
            with pl.platform("Windows"):
                with pl.compiler("msvc"):
                    pl.add_definitions("_DEBUG")
                    pl.add_compiler_flags("-Zc:preprocessor", "-nologo", "-std:c11", "-W4", "-WX", "-wd4201")
                    pl.add_compiler_flags("-wd4100", "-wd4996", "-wd4505", "-wd4189", "-wd5105", "-wd4115", "-permissive-")
                    pl.add_compiler_flags("-Od", "-MDd", "-Zi")

            # linux
            with pl.platform("Linux"):
                with pl.compiler("gcc"):
                    pl.add_link_directories("/usr/lib/x86_64-linux-gnu")
                    pl.add_dynamic_link_libraries("pthread")
                    pl.add_compiler_flags("-std=gnu11", "-fPIC", "--debug", "-g")
                    pl.add_linker_flags("-ldl", "-lm")

            # macos
            with pl.platform("Darwin"):
                with pl.compiler("clang"):
                    pl.add_compiler_flags("-std=c99", "--debug", "-g", "-fmodules", "-ObjC", "-fPIC")
                    pl.add_link_frameworks("Metal", "MetalKit", "Cocoa", "IOKit", "CoreVideo", "QuartzCore")
                    pl.add_linker_flags("-Wl,-rpath,/usr/local/lib")

#-----------------------------------------------------------------------------
# [SECTION] generate scripts
#-----------------------------------------------------------------------------

win32.generate_build(working_directory + '/' + "build_win32.bat", {"dev env setup" : True})
linux.generate_build(working_directory + '/' + "build_linux.sh")
apple.generate_build(working_directory + '/' + "build_macos.sh")