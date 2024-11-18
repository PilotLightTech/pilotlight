
:: Project: pilotlight_tests
:: Auto Generated by:
:: "pl_build.py" version: 1.0.12

:: Project: pilotlight_tests

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

:: hack to see if ../out/pilot_light_test exe is running
@echo off
2>nul (>>"../out/pilot_light_test.exe" echo off) && (@set PL_HOT_RELOAD_STATUS=0) || (@set PL_HOT_RELOAD_STATUS=1)

:: let user know if hot reloading
@if %PL_HOT_RELOAD_STATUS% equ 1 (
    @echo [1m[97m[41m--------[42m HOT RELOADING [41m--------[0m
)

:: cleanup binaries if not hot reloading
@if %PL_HOT_RELOAD_STATUS% equ 0 (

    @if exist "../out/pilot_light_test.exe" del "..\out\pilot_light_test.exe"
    @if exist "../out/pilot_light_test_*.pdb" del "..\out\pilot_light_test_*.pdb"

)

::~~~~~~~~~~~~~~~~~~~~~~~~~~~ pilot_light_test | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~

:: skip during hot reload
@if %PL_HOT_RELOAD_STATUS% equ 1 goto Exit_pilot_light_test

@set PL_DEFINES=-DPL_CONFIG_DEBUG -D_DEBUG 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -Od -MDd -Zi 
@set PL_LINKER_FLAGS=-incremental:no 
@set PL_SOURCES="main_tests.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: pilot_light_test[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m

:: skip actual compilation if hot reloading
@if %PL_HOT_RELOAD_STATUS% equ 1 ( goto Cleanuppilot_light_test )

:: call compiler
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/pilot_light_test.exe" -Fo"../out/" -link %PL_LINKER_FLAGS% -PDB:"../out/pilot_light_test_%random%.pdb" %PL_LINK_DIRECTORIES%

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

:Exit_pilot_light_test

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

:: hack to see if ../out/pilot_light_test exe is running
@echo off
2>nul (>>"../out/pilot_light_test.exe" echo off) && (@set PL_HOT_RELOAD_STATUS=0) || (@set PL_HOT_RELOAD_STATUS=1)

:: let user know if hot reloading
@if %PL_HOT_RELOAD_STATUS% equ 1 (
    @echo [1m[97m[41m--------[42m HOT RELOADING [41m--------[0m
)

:: cleanup binaries if not hot reloading
@if %PL_HOT_RELOAD_STATUS% equ 0 (

    @if exist "../out/pilot_light_test.exe" del "..\out\pilot_light_test.exe"
    @if exist "../out/pilot_light_test_*.pdb" del "..\out\pilot_light_test_*.pdb"

)

::~~~~~~~~~~~~~~~~~~~~~~~~~~ pilot_light_test | release ~~~~~~~~~~~~~~~~~~~~~~~~~~

:: skip during hot reload
@if %PL_HOT_RELOAD_STATUS% equ 1 goto Exit_pilot_light_test

@set PL_DEFINES=-DPL_CONFIG_RELEASE 
@set PL_INCLUDE_DIRECTORIES=-I"../examples" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -O2 -MD 
@set PL_LINKER_FLAGS=-incremental:no 
@set PL_SOURCES="main_tests.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: pilot_light_test[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m

:: skip actual compilation if hot reloading
@if %PL_HOT_RELOAD_STATUS% equ 1 ( goto Cleanuppilot_light_test )

:: call compiler
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/pilot_light_test.exe" -Fo"../out/" -link %PL_LINKER_FLAGS% -PDB:"../out/pilot_light_test_%random%.pdb" %PL_LINK_DIRECTORIES%

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

:Exit_pilot_light_test

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
