import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/../pl_build")

import pl_build as pl

pl.register_standard_profiles()

with pl.project("pilotlight"):
    
    # configurations
    pl.add_configuration("debug")

    # where to output build scripts
    pl.set_working_directory(os.path.dirname(os.path.abspath(__file__)) + "/../tests")

    # used to decide hot reloading
    pl.set_main_target("pilot_light_test")

    pl.push_profile(pl.Profile.PILOT_LIGHT_DEBUG_C)

    pl.push_definitions("_USE_MATH_DEFINES", "PL_PROFILING_ON", "PL_ALLOW_HOT_RELOAD", "PL_ENABLE_VALIDATION_LAYERS")
    pl.push_include_directories("../apps", "../src", "../libs", "../extensions", "../out", "../dependencies/stb", "../dependencies/cgltf")
    pl.push_link_directories("../out")
    pl.push_output_directory("../out")
        
    ###############################################################################
    #                                 tests                                       #
    ###############################################################################
    with pl.target("pilot_light_test", pl.TargetType.EXECUTABLE):

        pl.push_output_binary("pilot_light_test")
        pl.push_source_files("main_tests.c")
               
        with pl.configuration("debug"):
            with pl.platform(pl.PlatformType.WIN32):
                with pl.compiler("msvc", pl.CompilerType.MSVC):
                    pass     
            with pl.platform(pl.PlatformType.LINUX):
                with pl.compiler("gcc", pl.CompilerType.GCC):
                    pass
            with pl.platform(pl.PlatformType.MACOS):
                with pl.compiler("clang", pl.CompilerType.CLANG):
                    pass
                
        pl.pop_source_files()
        pl.pop_output_binary()


    pl.pop_definitions()
    pl.pop_include_directories()
    pl.pop_link_directories()
    pl.pop_output_directory()  
    pl.pop_profile()

pl.generate_build_script("build")