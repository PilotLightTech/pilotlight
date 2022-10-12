@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@rem |                            Setup                                       |
@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@rem without this, PATH will not reset when called within same session
@setlocal 

@rem make current directory the same as batch file
@pushd %~dp0 
@set dir=%~dp0

@rem create output directory
@if not exist ..\out @mkdir ..\out

@rem cleanup temp files
@del ..\out\*.pdb > NUL 2> NUL
@del ..\out\*.log > NUL 2> NUL

@rem -------------------Setup development environment--------------------------
@set PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=%dir%..\out;%PATH%

@rem setup environment for msvc
@call vcvarsall.bat amd64

@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@rem |                          Common Settings                               |
@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@rem build config: Debug or Release
@set PL_CONFIG=Debug

@rem include directories
@set PL_INCLUDE_DIRECTORIES=-I"%WindowsSdkDir%Include\um" 
@set PL_INCLUDE_DIRECTORIES=-I"%WindowsSdkDir%Include\shared" %PL_INCLUDE_DIRECTORIES%
@set PL_INCLUDE_DIRECTORIES=-I%VULKAN_SDK%/Include            %PL_INCLUDE_DIRECTORIES%
@set PL_INCLUDE_DIRECTORIES=-I..\dependencies\stb             %PL_INCLUDE_DIRECTORIES%

@rem link directories
@set PL_LINK_DIRECTORIES=-LIBPATH:"%VULKAN_SDK%\Lib" -LIBPATH:"..\out"

@rem common defines
@set PL_DEFINES=-D_USE_MATH_DEFINES

@rem common compiler flags
@set PL_COMPILER_FLAGS=-Zc:preprocessor -nologo -std:c11 -EHsc -W4 -WX -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105

@rem common libraries
@set PL_LINK_LIBRARIES=user32.lib ws2_32.lib shlwapi.lib propsys.lib comctl32.lib Shell32.lib Ole32.lib vulkan-1.lib pl.lib

@rem release specific
@if "%PL_CONFIG%" EQU "Release" (

    @rem release specific defines
    @set PL_DEFINES=%PL_DEFINES%

    @rem release specific compiler flags
    @set PL_COMPILER_FLAGS=-O2 -MD %PL_COMPILER_FLAGS%

    @rem release specific libs
    @set PL_LINK_LIBRARIES=ucrt.lib %PL_LINK_LIBRARIES%
)

@rem debug specific
@if "%PL_CONFIG%" EQU "Debug" (

    @rem debug specific defines
    @set PL_DEFINES=-DPL_PROFILING_ON -D_DEBUG -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS %PL_DEFINES%
   
    @rem debug specific compiler flags
    @set PL_COMPILER_FLAGS=-Od -MDd -Zi %PL_COMPILER_FLAGS%
    
    @rem debug specific libs
    @set PL_LINK_LIBRARIES=ucrtd.lib %PL_LINK_LIBRARIES%
)

@set PL_RESULT=[1m[92mSuccessful.[0m

@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@rem |                          pl lib                                       |
@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@set PL_SOURCES=pl.c

@rem run compiler
cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% -c -permissive- %PL_SOURCES% -Fe..\out\pl.lib -Fo..\out\

@rem check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%
@if %PL_BUILD_STATUS% NEQ 0 (
    echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanup2
)
@echo [1m[36mCompiled successfully. Now linking...[0m

@rem link object files into a shared lib
lib -nologo -OUT:..\out\pl.lib ..\out\*.obj

:Cleanup2
    @echo [1m[36mCleaning up intermediate files...[0m
    @del ..\out\*.obj

@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@rem |                          Executable                                    |
@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
:MainBuild

@set PL_SOURCES=win32_pl.c

@rem run compiler
@cl %PL_INCLUDE_DIRECTORIES% %PL_DEFINES% %PL_COMPILER_FLAGS% -permissive- %PL_SOURCES% -Fe..\out\pilot_light.exe -Fo..\out\ -link -incremental:no %PL_LINKER_FLAGS% %PL_LINK_DIRECTORIES% %PL_LINK_LIBRARIES%

@rem check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%
@if %PL_BUILD_STATUS% NEQ 0 (
    echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanup1
)

:Cleanup1
    @echo [1m[36mCleaning up intermediate files...[0m
    @del ..\out\*.obj

@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@rem |                          Shaders                                       |
@rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@REM %VULKAN_SDK%/bin/glslc -o ../out/simple.frag.spv ./shaders/simple.frag
@REM %VULKAN_SDK%/bin/glslc -o ../out/simple.vert.spv ./shaders/simple.vert

@rem --------------------------------------------------------------------------
@rem Information Output
@rem --------------------------------------------------------------------------
:PrintInfo
@echo [36m--------------------------------------------------------------------------[0m
@echo [1m[93m                        Build Information [0m
@echo [36mResults:             [0m %PL_RESULT%
@echo [36mConfiguration:       [0m [35m%PL_CONFIG%[0m
@echo [36mWorking directory:   [0m [35m%dir%[0m
@echo [36mOutput directory:    [0m [35m../out[0m
@echo [36mOutput binary:       [0m [33mpilot_light.exe[0m
@echo [36m--------------------------------------------------------------------------[0m

@popd

@rem keep terminal open if clicked from explorer
@echo off
for %%x in (%cmdcmdline%) do if /i "%%~x"=="/c" set DOUBLECLICKED=1
if defined DOUBLECLICKED pause
