# gen_build.py

# Index of this file:
# [SECTION] imports
# [SECTION] project
# [SECTION] profiles
# [SECTION] pilot_light
# [SECTION] extensions
# [SECTION] ecs scripts
# [SECTION] platform extension
# [SECTION] imgui & implot
# [SECTION] app
# [SECTION] generate_scripts

#-----------------------------------------------------------------------------
# [SECTION] imports
#-----------------------------------------------------------------------------

import os
import sys
import platform as plat

try:
    import pl_build.core as pl
    import pl_build.backend_win32 as win32
    import pl_build.backend_linux as linux
    import pl_build.backend_macos as apple
except ImportError:
    # just use the packaged build so users don't need to pip install pl-build
    if len(sys.argv) > 1:
        sys.path.insert(0, sys.argv[1])
    import build.core as pl
    import build.backend_win32 as win32
    import build.backend_linux as linux
    import build.backend_macos as apple

#-----------------------------------------------------------------------------
# [SECTION] project
#-----------------------------------------------------------------------------

output_directory = "../out"

with pl.project("app"):
    
    # used to decide hot reloading
    pl.add_hot_reload_target(output_directory + "/pilot_light")
    pl.set_hot_reload_artifact_directory(output_directory + "/../out-temp")

    # project wide settings
    pl.set_output_directory(output_directory)
    pl.add_link_directories(output_directory)
    pl.add_include_directories(output_directory,
                               "../src", "../shaders", "../extensions", "../dependencies/pilotlight/shaders",
                               "../dependencies/pilotlight/include",
                               "../dependencies/pilotlight/src")

    #-----------------------------------------------------------------------------
    # [SECTION] app
    #-----------------------------------------------------------------------------

    with pl.target("app", pl.TargetType.DYNAMIC_LIBRARY, reloadable=True):

        pl.add_source_files("../src/app.cpp")
        pl.set_output_binary("app")

        # default config
        with pl.configuration("debug"):

            pl.add_definitions("PL_CONFIG_DEBUG")

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

        with pl.configuration("release"):

            pl.add_definitions("PL_CONFIG_RELEASE")

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
            
#-----------------------------------------------------------------------------
# [SECTION] generate scripts
#-----------------------------------------------------------------------------

# where to output build scripts
working_directory = os.path.dirname(os.path.abspath(__file__)) + "/../build"

win32.generate_build(working_directory + '/' + "build_app_win32.bat")
apple.generate_build(working_directory + '/' + "build_app_macos.sh")
linux.generate_build(working_directory + '/' + "build_app_linux.sh")