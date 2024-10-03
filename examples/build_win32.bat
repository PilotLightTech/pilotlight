
:: Project: pilotlight_examples
:: Auto Generated by:
:: "pl_build.py" version: 1.0.9

:: Project: pilotlight_examples

:: ################################################################################
:: #                              Development Setup                               #
:: ################################################################################

:: keep environment variables modifications local
@setlocal

:: make script directory CWD
@pushd %~dp0
@set dir=%~dp0

:: modify PATH to find vcvarsall.bat
@if exist "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build" @set PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@if exist "C:/Program Files/Microsoft Visual Studio/2019/Community/VC/Auxiliary/Build" @set PATH=C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@if exist "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Auxiliary/Build" @set PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build;%PATH%
@if exist "C:/Program Files/Microsoft Visual Studio/2019/Professional/VC/Auxiliary/Build" @set PATH=C:\Program Files\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build;%PATH%
@if exist "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Auxiliary/Build" @set PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build;%PATH%
@if exist "C:/Program Files/Microsoft Visual Studio/2019/Enterprise/VC/Auxiliary/Build" @set PATH=C:\Program Files\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build;%PATH%
@if exist "C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build" @set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@if exist "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Auxiliary/Build" @set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@if exist "C:/Program Files (x86)/Microsoft Visual Studio/2022/Professional/VC/Auxiliary/Build" @set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build;%PATH%
@if exist "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/VC/Auxiliary/Build" @set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build;%PATH%
@if exist "C:/Program Files (x86)/Microsoft Visual Studio/2022/Enterprise/VC/Auxiliary/Build" @set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build;%PATH%
@if exist "C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/VC/Auxiliary/Build" @set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build;%PATH%

:: setup environment for MSVC dev tools
@call vcvarsall.bat amd64 > nul

:: default compilation result
@set PL_RESULT=[1m[92mSuccessful.[0m

:: default configuration
@set PL_CONFIG=debug

:: check command line args for configuration
:CheckConfiguration
@if "%~1"=="-c" (@set PL_CONFIG=%2) & @shift & @shift & @goto CheckConfiguration
@if "%PL_CONFIG%" equ "debug" ( goto debug )
@if "%PL_CONFIG%" equ "release" ( goto release )

:: ################################################################################
:: #                            configuration | debug                             #
:: ################################################################################

:debug

:: create output directories
@if not exist "../out" @mkdir "../out"

:: create lock file(s)
@echo LOCKING > "../out/lock.tmp"

:: check if this is a hot reload
@set PL_HOT_RELOAD_STATUS=0

:: hack to see if ../out/pilot_light exe is running
@echo off
2>nul (>>"../out/pilot_light.exe" echo off) && (@set PL_HOT_RELOAD_STATUS=0) || (@set PL_HOT_RELOAD_STATUS=1)

:: let user know if hot reloading
@if %PL_HOT_RELOAD_STATUS% equ 1 (
    @echo [1m[97m[41m--------[42m HOT RELOADING [41m--------[0m
)

:: cleanup binaries if not hot reloading
@if %PL_HOT_RELOAD_STATUS% equ 0 (

    @if exist "../out/example_0.dll" del "..\out\example_0.dll"
    @if exist "../out/example_0_*.dll" del "..\out\example_0_*.dll"
    @if exist "../out/example_0_*.pdb" del "..\out\example_0_*.pdb"
    @if exist "../out/example_1.dll" del "..\out\example_1.dll"
    @if exist "../out/example_1_*.dll" del "..\out\example_1_*.dll"
    @if exist "../out/example_1_*.pdb" del "..\out\example_1_*.pdb"
    @if exist "../out/example_2.dll" del "..\out\example_2.dll"
    @if exist "../out/example_2_*.dll" del "..\out\example_2_*.dll"
    @if exist "../out/example_2_*.pdb" del "..\out\example_2_*.pdb"
    @if exist "../out/example_3.dll" del "..\out\example_3.dll"
    @if exist "../out/example_3_*.dll" del "..\out\example_3_*.dll"
    @if exist "../out/example_3_*.pdb" del "..\out\example_3_*.pdb"
    @if exist "../out/example_4.dll" del "..\out\example_4.dll"
    @if exist "../out/example_4_*.dll" del "..\out\example_4_*.dll"
    @if exist "../out/example_4_*.pdb" del "..\out\example_4_*.pdb"
    @if exist "../out/example_5.dll" del "..\out\example_5.dll"
    @if exist "../out/example_5_*.dll" del "..\out\example_5_*.dll"
    @if exist "../out/example_5_*.pdb" del "..\out\example_5_*.pdb"
    @if exist "../out/example_6.dll" del "..\out\example_6.dll"
    @if exist "../out/example_6_*.dll" del "..\out\example_6_*.dll"
    @if exist "../out/example_6_*.pdb" del "..\out\example_6_*.pdb"
    @if exist "../out/example_8.dll" del "..\out\example_8.dll"
    @if exist "../out/example_8_*.dll" del "..\out\example_8_*.dll"
    @if exist "../out/example_8_*.pdb" del "..\out\example_8_*.pdb"

)

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_0 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -Od -MDd -Zi 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_0.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_0[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_0.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_0_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanupdebug
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_1 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -Od -MDd -Zi 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_1.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_1[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_1.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_1_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanupdebug
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_2 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -Od -MDd -Zi 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_2.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_2[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_2.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_2_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanupdebug
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_3 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -Od -MDd -Zi 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_3.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_3[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_3.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_3_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanupdebug
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_4 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -Od -MDd -Zi 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_4.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_4[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_4.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_4_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanupdebug
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_5 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -Od -MDd -Zi 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_5.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_5[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_5.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_5_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanupdebug
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_6 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -Od -MDd -Zi 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_6.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_6[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_6.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_6_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanupdebug
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_8 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -Od -MDd -Zi 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_8.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_8[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_8.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_8_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanupdebug
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

:Cleanupdebug

@echo [1m[36mCleaning...[0m

:: delete obj files(s)
@del "..\out\*.obj"  > nul 2> nul

:: delete lock file(s)
@if exist "../out/lock.tmp" del "..\out\lock.tmp"

:: ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
:: end of debug configuration
goto ExitLabel

:: ################################################################################
:: #                           configuration | release                            #
:: ################################################################################

:release

:: create output directories
@if not exist "../out" @mkdir "../out"

:: create lock file(s)
@echo LOCKING > "../out/lock.tmp"

:: check if this is a hot reload
@set PL_HOT_RELOAD_STATUS=0

:: hack to see if ../out/pilot_light exe is running
@echo off
2>nul (>>"../out/pilot_light.exe" echo off) && (@set PL_HOT_RELOAD_STATUS=0) || (@set PL_HOT_RELOAD_STATUS=1)

:: let user know if hot reloading
@if %PL_HOT_RELOAD_STATUS% equ 1 (
    @echo [1m[97m[41m--------[42m HOT RELOADING [41m--------[0m
)

:: cleanup binaries if not hot reloading
@if %PL_HOT_RELOAD_STATUS% equ 0 (

    @if exist "../out/example_0.dll" del "..\out\example_0.dll"
    @if exist "../out/example_0_*.dll" del "..\out\example_0_*.dll"
    @if exist "../out/example_0_*.pdb" del "..\out\example_0_*.pdb"
    @if exist "../out/example_1.dll" del "..\out\example_1.dll"
    @if exist "../out/example_1_*.dll" del "..\out\example_1_*.dll"
    @if exist "../out/example_1_*.pdb" del "..\out\example_1_*.pdb"
    @if exist "../out/example_2.dll" del "..\out\example_2.dll"
    @if exist "../out/example_2_*.dll" del "..\out\example_2_*.dll"
    @if exist "../out/example_2_*.pdb" del "..\out\example_2_*.pdb"
    @if exist "../out/example_3.dll" del "..\out\example_3.dll"
    @if exist "../out/example_3_*.dll" del "..\out\example_3_*.dll"
    @if exist "../out/example_3_*.pdb" del "..\out\example_3_*.pdb"
    @if exist "../out/example_4.dll" del "..\out\example_4.dll"
    @if exist "../out/example_4_*.dll" del "..\out\example_4_*.dll"
    @if exist "../out/example_4_*.pdb" del "..\out\example_4_*.pdb"
    @if exist "../out/example_5.dll" del "..\out\example_5.dll"
    @if exist "../out/example_5_*.dll" del "..\out\example_5_*.dll"
    @if exist "../out/example_5_*.pdb" del "..\out\example_5_*.pdb"
    @if exist "../out/example_6.dll" del "..\out\example_6.dll"
    @if exist "../out/example_6_*.dll" del "..\out\example_6_*.dll"
    @if exist "../out/example_6_*.pdb" del "..\out\example_6_*.pdb"
    @if exist "../out/example_8.dll" del "..\out\example_8.dll"
    @if exist "../out/example_8_*.dll" del "..\out\example_8_*.dll"
    @if exist "../out/example_8_*.pdb" del "..\out\example_8_*.pdb"

)

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_0 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -O2 -MD 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_0.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_0[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_0.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_0_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanuprelease
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_1 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -O2 -MD 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_1.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_1[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_1.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_1_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanuprelease
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_2 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -O2 -MD 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_2.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_2[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_2.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_2_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanuprelease
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_3 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -O2 -MD 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_3.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_3[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_3.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_3_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanuprelease
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_4 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -O2 -MD 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_4.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_4[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_4.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_4_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanuprelease
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_5 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -O2 -MD 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_5.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_5[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_5.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_5_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanuprelease
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_6 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -O2 -MD 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_6.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_6[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_6.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_6_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanuprelease
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_8 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -O2 -MD 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="example_8.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: example_8[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/example_8.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/example_8_%random%.pdb" %PL_LINK_DIRECTORIES%

:: check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

:: failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanuprelease
)

:: print results
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

:Cleanuprelease

@echo [1m[36mCleaning...[0m

:: delete obj files(s)
@del "..\out\*.obj"  > nul 2> nul

:: delete lock file(s)
@if exist "../out/lock.tmp" del "..\out\lock.tmp"

:: ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
:: end of release configuration
goto ExitLabel

:ExitLabel

:: return CWD to previous CWD
@popd
