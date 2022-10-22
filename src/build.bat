@if [%1]==[] @goto Run
@goto Run

@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@rem |                            Help Message                                |
@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Usage
@echo.
@echo.[flags and arguments]
@echo.
@echo.Available flags:
@echo.  -h  Display this help message
@echo.
@echo.Available arguments:
@echo.  -g dx11 ^| vulkan ^|
@echo.     Set graphics backend (default: vulkan)
@exit /b 127

:Run

@rem without this, PATH will not reset when called within same session
@setlocal 

@rem make current directory the same as batch file
@pushd %~dp0 
@set dir=%~dp0

@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@rem |                            Command Line                                |
@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@rem default backend to vulkan
@set PL_BACKEND=vulkan

:CheckOpts
@if "%~1"=="-h" @goto Usage
@if "%~1"=="-i" @goto PrintInfo
@if "%~1"=="-g" (@set PL_BACKEND=%2) & @shift & @shift & @goto CheckOpts

@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@rem |                            Setup                                       |
@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@rem create output directory
@if not exist ..\out @mkdir ..\out

@rem cleanup temp files
@del ..\out\*.pdb > NUL 2> NUL
@del ..\out\*.log > NUL 2> NUL

@echo LOCKING > ..\out\lock.tmp

@rem check if hot reloading
@set PL_HOT_RELOADING_STATUS=0
@echo off
2>nul (>>..\out\pilot_light.exe echo off) && (@set PL_HOT_RELOADING_STATUS=0) || (@set PL_HOT_RELOADING_STATUS=1)

@if %PL_HOT_RELOADING_STATUS% equ 1 (
    @echo. 
    @echo [1m[97m[41m--------[42m HOT RELOADING [41m--------[0m
)

@if %PL_HOT_RELOADING_STATUS% equ 0 (
    @if exist ..\out\app_*.dll del ..\out\app_*.dll
    @if exist ..\out\app_*.pdb del ..\out\app_*.pdb
)

@rem -------------------Setup development environment--------------------------
@set PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build;%PATH%
@set PATH=%dir%..\out;%PATH%

@rem setup environment for msvc
@call vcvarsall.bat amd64 > nul

@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@rem |                          Common Settings                               |
@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@rem build config: Debug or Release
@set PL_CONFIG=Debug

@rem include directories
@set PL_INCLUDE_DIRECTORIES=-I"%WindowsSdkDir%Include\um" 
@set PL_INCLUDE_DIRECTORIES=-I"%WindowsSdkDir%Include\shared" %PL_INCLUDE_DIRECTORIES%
@set PL_INCLUDE_DIRECTORIES=-I"%DXSDK_DIR%Include"            %PL_INCLUDE_DIRECTORIES%
@set PL_INCLUDE_DIRECTORIES=-I%VULKAN_SDK%/Include            %PL_INCLUDE_DIRECTORIES%
@set PL_INCLUDE_DIRECTORIES=-I..\dependencies\stb             %PL_INCLUDE_DIRECTORIES%

@rem link directories
@set PL_LINK_DIRECTORIES=-LIBPATH:"%VULKAN_SDK%\Lib" -LIBPATH:"..\out"

@rem common defines
@set PL_DEFINES=-D_USE_MATH_DEFINES

@rem common compiler flags
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -EHsc -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115

@rem common libraries
@set PL_LINK_LIBRARIES=pilotlight.lib

@rem release specific
@if "%PL_CONFIG%" equ "Release" (

    @rem release specific defines
    @set PL_DEFINES=%PL_DEFINES%

    @rem release specific compiler flags
    @set PL_COMPILER_FLAGS=-O2 -MD %PL_COMPILER_FLAGS%
)

@rem debug specific
@if "%PL_CONFIG%" equ "Debug" (

    @rem debug specific defines
    @set PL_DEFINES=-DPL_PROFILING_ON -D_DEBUG -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS %PL_DEFINES%
   
    @rem debug specific compiler flags
    @set PL_COMPILER_FLAGS=-Od -MDd -Zi %PL_COMPILER_FLAGS%
)

@if "%PL_BACKEND%" equ "dx11" (
    @set PL_DEFINES=-DPL_DX11_BACKEND %PL_DEFINES%
)

@if "%PL_BACKEND%" equ "vulkan" (
    @set PL_DEFINES=-DPL_VULKAN_BACKEND %PL_DEFINES%
)

@set PL_RESULT=[1m[92mSuccessful.[0m

@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@rem |                          Shaders                                       |
@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@rem compile shaders
@echo.
@echo [1m[93mStep 0: shaders[0m
@echo [1m[93m~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling...[0m
@REM %VULKAN_SDK%/bin/glslc -o ../out/simple.frag.spv ./shaders/simple.frag
@REM %VULKAN_SDK%/bin/glslc -o ../out/simple.vert.spv ./shaders/simple.vert

@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@rem |                          pl lib                                       |
@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_SOURCES=pilotlight.c

@rem run compiler
@echo.
@echo [1m[93mStep 1: pilotlight.lib[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% -c -permissive- %PL_SOURCES% -Fe..\out\pilotlight.lib -Fo..\out\

@rem check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto CleanupPlLib
)
@echo [1m[36mLinking...[0m

@rem link object files into a shared lib
lib -nologo -OUT:..\out\pilotlight.lib ..\out\*.obj

:CleanupPlLib
    @echo [1m[36mCleaning...[0m
    @del ..\out\*.obj

@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@rem |                         app lib                                        |
@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@if "%PL_BACKEND%" equ "dx11"   ( @set PL_SOURCES="app_dx11.c" )
@if "%PL_BACKEND%" equ "vulkan" ( @set PL_SOURCES="app_vulkan.c" )

@rem run compiler
@echo.
@echo [1m[93mStep 2: app.dll[0m
@echo [1m[93m~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling...[0m
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% -permissive- %PL_SOURCES% -Fe..\out\app.dll -Fo..\out\ -LD -link -noimplib -noexp -incremental:no -PDB:..\out\app_%random%.pdb %PL_LINKER_FLAGS% %PL_LINK_DIRECTORIES% %PL_LINK_LIBRARIES%

@rem check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%
@if %PL_BUILD_STATUS% NEQ 0 (
    echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto CleanupApp
)

:CleanupApp
    @echo [1m[36mCleaning...[0m
    @del ..\out\*.obj

@if %PL_HOT_RELOADING_STATUS% equ 1 ( goto PrintInfo )

@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@rem |                          Executable                                    |
@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
:MainBuild

@set PL_SOURCES="pl_main_win32.c"

@rem run compiler
@echo.
@echo [1m[93mStep 3: pilot_light.exe[0m
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m
@cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% -permissive- %PL_SOURCES% -Fe..\out\pilot_light.exe -Fo..\out\ -link -incremental:no %PL_LINKER_FLAGS% %PL_LINK_DIRECTORIES% %PL_LINK_LIBRARIES%

@rem check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

@if %PL_BUILD_STATUS% neq 0 (
    echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto CleanupExe
)

:CleanupExe
    @echo [1m[36mCleaning...[0m
    @del ..\out\*.obj

@rem --------------------------------------------------------------------------
@rem Information Output
@rem --------------------------------------------------------------------------
:PrintInfo
@echo.
@echo [36m--------------------------------------------------------------------------[0m
@echo [1m[93m                        Build Information [0m
@echo [36mResults:             [0m %PL_RESULT%
@echo [36mConfiguration:       [0m [35m%PL_CONFIG%[0m
@echo [36mWorking directory:   [0m [35m%dir%[0m
@echo [36mOutput directory:    [0m [35m..\out[0m
@echo [36mOutput binary:       [0m [33mpilot_light.exe[0m
@echo [36m--------------------------------------------------------------------------[0m

del ..\out\lock.tmp

@popd

@rem keep terminal open if clicked from explorer
@echo off
for %%x in (%cmdcmdline%) do if /i "%%~x"=="/c" set DOUBLECLICKED=1
if defined DOUBLECLICKED pause
