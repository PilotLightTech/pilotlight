
:: Project: pilotlight
:: Auto Generated by:
:: "pl_build.py" version: 1.0.9

:: Project: pilotlight

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
@if "%PL_CONFIG%" equ "vulkan" ( goto vulkan )

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

    @if exist "../out/pilot_light.dll" del "..\out\pilot_light.dll"
    @if exist "../out/pilot_light_*.dll" del "..\out\pilot_light_*.dll"
    @if exist "../out/pilot_light_*.pdb" del "..\out\pilot_light_*.pdb"
    @if exist "../out/pl_script_camera.dll" del "..\out\pl_script_camera.dll"
    @if exist "../out/pl_script_camera_*.dll" del "..\out\pl_script_camera_*.dll"
    @if exist "../out/pl_script_camera_*.pdb" del "..\out\pl_script_camera_*.pdb"
    @if exist "../out/app.dll" del "..\out\app.dll"
    @if exist "../out/app_*.dll" del "..\out\app_*.dll"
    @if exist "../out/app_*.pdb" del "..\out\app_*.pdb"
    @if exist "../out/pilot_light.exe" del "..\out\pilot_light.exe"
    @if exist "../out/pilot_light_*.pdb" del "..\out\pilot_light_*.pdb"

)

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~ pl_extensions | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-DPL_VULKAN_BACKEND -D_DEBUG 
@set PL_INCLUDE_DIRECTORIES=-I"../sandbox" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" -I"../dependencies/cgltf" -I"%WindowsSdkDir%Include\um" -I"%WindowsSdkDir%Include\shared" -I"%VULKAN_SDK%\Include" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" -LIBPATH:"%VULKAN_SDK%\Lib" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -Od -MDd -Zi 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no -nodefaultlib:MSVCRT 
@set PL_STATIC_LINK_LIBRARIES=shaderc_combined.lib vulkan-1.lib 
@set PL_SOURCES="pl_extensions.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: pl_extensions[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/pilot_light.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/pilot_light_%random%.pdb" %PL_LINK_DIRECTORIES% %PL_STATIC_LINK_LIBRARIES%

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

::~~~~~~~~~~~~~~~~~~~~~~~~~~~ pl_script_camera | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-DPL_VULKAN_BACKEND -D_DEBUG 
@set PL_INCLUDE_DIRECTORIES=-I"../sandbox" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" -I"../dependencies/cgltf" -I"%WindowsSdkDir%Include\um" -I"%WindowsSdkDir%Include\shared" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -Od -MDd -Zi 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="../extensions/pl_script_camera.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: pl_script_camera[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/pl_script_camera.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/pl_script_camera_%random%.pdb" %PL_LINK_DIRECTORIES%

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

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ app | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-DPL_VULKAN_BACKEND -D_DEBUG 
@set PL_INCLUDE_DIRECTORIES=-I"../sandbox" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" -I"../dependencies/cgltf" -I"%WindowsSdkDir%Include\um" -I"%WindowsSdkDir%Include\shared" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -Od -MDd -Zi 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="../sandbox/app.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: app[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/app.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/app_%random%.pdb" %PL_LINK_DIRECTORIES%

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

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ pilot_light | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:: skip during hot reload
@if %PL_HOT_RELOAD_STATUS% equ 1 goto Exit_pilot_light

@set PL_DEFINES=-DPL_VULKAN_BACKEND -D_DEBUG 
@set PL_INCLUDE_DIRECTORIES=-I"../sandbox" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" -I"../dependencies/cgltf" -I"%WindowsSdkDir%Include\um" -I"%WindowsSdkDir%Include\shared" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -Od -MDd -Zi 
@set PL_LINKER_FLAGS=-incremental:no 
@set PL_STATIC_LINK_LIBRARIES=ucrtd.lib user32.lib Ole32.lib ws2_32.lib 
@set PL_SOURCES="pl_main_win32.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: pilot_light[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m

:: skip actual compilation if hot reloading
@if %PL_HOT_RELOAD_STATUS% equ 1 ( goto Cleanuppilot_light )

:: call compiler
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/pilot_light.exe" -Fo"../out/" -link %PL_LINKER_FLAGS% -PDB:"../out/pilot_light_%random%.pdb" %PL_LINK_DIRECTORIES% %PL_STATIC_LINK_LIBRARIES%

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

:Exit_pilot_light

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

    @if exist "../out/pilot_light.dll" del "..\out\pilot_light.dll"
    @if exist "../out/pilot_light_*.dll" del "..\out\pilot_light_*.dll"
    @if exist "../out/pilot_light_*.pdb" del "..\out\pilot_light_*.pdb"
    @if exist "../out/pl_script_camera.dll" del "..\out\pl_script_camera.dll"
    @if exist "../out/pl_script_camera_*.dll" del "..\out\pl_script_camera_*.dll"
    @if exist "../out/pl_script_camera_*.pdb" del "..\out\pl_script_camera_*.pdb"
    @if exist "../out/app.dll" del "..\out\app.dll"
    @if exist "../out/app_*.dll" del "..\out\app_*.dll"
    @if exist "../out/app_*.pdb" del "..\out\app_*.pdb"
    @if exist "../out/pilot_light.exe" del "..\out\pilot_light.exe"
    @if exist "../out/pilot_light_*.pdb" del "..\out\pilot_light_*.pdb"

)

::~~~~~~~~~~~~~~~~~~~~~~~~~~~ pl_extensions | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-DPL_VULKAN_BACKEND -DNDEBUG 
@set PL_INCLUDE_DIRECTORIES=-I"../sandbox" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" -I"../dependencies/cgltf" -I"%WindowsSdkDir%Include\um" -I"%WindowsSdkDir%Include\shared" -I"%VULKAN_SDK%\Include" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" -LIBPATH:"%VULKAN_SDK%\Lib" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -O2 -MD 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_STATIC_LINK_LIBRARIES=shaderc_combined.lib vulkan-1.lib 
@set PL_SOURCES="pl_extensions.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: pl_extensions[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/pilot_light.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/pilot_light_%random%.pdb" %PL_LINK_DIRECTORIES% %PL_STATIC_LINK_LIBRARIES%

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

::~~~~~~~~~~~~~~~~~~~~~~~~~~ pl_script_camera | release ~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-DPL_VULKAN_BACKEND -DNDEBUG 
@set PL_INCLUDE_DIRECTORIES=-I"../sandbox" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" -I"../dependencies/cgltf" -I"%WindowsSdkDir%Include\um" -I"%WindowsSdkDir%Include\shared" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -O2 -MD 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="../extensions/pl_script_camera.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: pl_script_camera[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/pl_script_camera.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/pl_script_camera_%random%.pdb" %PL_LINK_DIRECTORIES%

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

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ app | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_DEFINES=-DPL_VULKAN_BACKEND -DNDEBUG 
@set PL_INCLUDE_DIRECTORIES=-I"../sandbox" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" -I"../dependencies/cgltf" -I"%WindowsSdkDir%Include\um" -I"%WindowsSdkDir%Include\shared" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -O2 -MD 
@set PL_LINKER_FLAGS=-noimplib -noexp -incremental:no 
@set PL_SOURCES="../sandbox/app.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: app[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/app.dll" -Fo"../out/" -LD -link %PL_LINKER_FLAGS% -PDB:"../out/app_%random%.pdb" %PL_LINK_DIRECTORIES%

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

::~~~~~~~~~~~~~~~~~~~~~~~~~~~~ pilot_light | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:: skip during hot reload
@if %PL_HOT_RELOAD_STATUS% equ 1 goto Exit_pilot_light

@set PL_DEFINES=-DPL_VULKAN_BACKEND -DNDEBUG 
@set PL_INCLUDE_DIRECTORIES=-I"../sandbox" -I"../src" -I"../libs" -I"../extensions" -I"../out" -I"../dependencies/stb" -I"../dependencies/cgltf" -I"%WindowsSdkDir%Include\um" -I"%WindowsSdkDir%Include\shared" 
@set PL_LINK_DIRECTORIES=-LIBPATH:"../out" 
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 -permissive- -O2 -MD 
@set PL_LINKER_FLAGS=-incremental:no 
@set PL_STATIC_LINK_LIBRARIES=ucrt.lib user32.lib Ole32.lib ws2_32.lib 
@set PL_SOURCES="pl_main_win32.c" 

:: run compiler (and linker)
@echo.
@echo [1m[93mStep: pilot_light[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m

:: skip actual compilation if hot reloading
@if %PL_HOT_RELOAD_STATUS% equ 1 ( goto Cleanuppilot_light )

:: call compiler
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% %PL_SOURCES% -Fe"../out/pilot_light.exe" -Fo"../out/" -link %PL_LINKER_FLAGS% -PDB:"../out/pilot_light_%random%.pdb" %PL_LINK_DIRECTORIES% %PL_STATIC_LINK_LIBRARIES%

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

:Exit_pilot_light

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
