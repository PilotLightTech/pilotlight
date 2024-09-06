
# Auto Generated by:
# "pl_build.py" version: 1.0.1

# Project: pilotlight_examples

# ################################################################################
# #                              Development Setup                               #
# ################################################################################

# colors
BOLD=$'\e[0;1m'
RED=$'\e[0;31m'
RED_BG=$'\e[0;41m'
GREEN=$'\e[0;32m'
GREEN_BG=$'\e[0;42m'
CYAN=$'\e[0;36m'
MAGENTA=$'\e[0;35m'
YELLOW=$'\e[0;33m'
WHITE=$'\e[0;97m'
NC=$'\e[0m'

# find directory of this script
SOURCE=${BASH_SOURCE[0]}
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
  SOURCE=$(readlink "$SOURCE")
  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

# get architecture (intel or apple silicon)
ARCH="$(uname -m)"

# make script directory CWD
pushd $DIR >/dev/null

# default configuration
PL_CONFIG=debug

# check command line args for configuration
while getopts ":c:" option; do
   case $option in
   c) # set conf
         PL_CONFIG=$OPTARG;;
     \?) # Invalid option
         echo "Error: Invalid option"
         exit;;
   esac
done

# ################################################################################
# #                            configuration | debug                             #
# ################################################################################

if [[ "$PL_CONFIG" == "debug" ]]; then

# create output directory(s)
mkdir -p "../out"

# create lock file(s)
echo LOCKING > "../out/lock.tmp"

# check if this is a reload
PL_HOT_RELOAD_STATUS=0

# # let user know if hot reloading
running_count=$(ps aux | grep -v grep | grep -ci "pilot_light")
if [ $running_count -gt 0 ]
then
    PL_HOT_RELOAD_STATUS=1
    echo
    echo echo ${BOLD}${WHITE}${RED_BG}--------${GREEN_BG} HOT RELOADING ${RED_BG}--------${NC}
    echo echo
else
    # cleanup binaries if not hot reloading
    echo PL_HOT_RELOAD_STATUS=0
    rm -f "../out/example_0.dylib"
    rm -f "../out/example_0_*.dylib"
    rm -f "../out/example_1.dylib"
    rm -f "../out/example_1_*.dylib"
    rm -f "../out/example_2.dylib"
    rm -f "../out/example_2_*.dylib"
    rm -f "../out/example_3.dylib"
    rm -f "../out/example_3_*.dylib"
    rm -f "../out/example_4.dylib"
    rm -f "../out/example_4_*.dylib"
    rm -f "../out/example_5.dylib"
    rm -f "../out/example_5_*.dylib"
    rm -f "../out/example_6.dylib"
    rm -f "../out/example_6_*.dylib"
    rm -f "../out/example_8.dylib"
    rm -f "../out/example_8_*.dylib"

fi
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_0 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out "
PL_COMPILER_FLAGS="-std=c99 --debug -g -fmodules -ObjC -fPIC "
PL_LINKER_FLAGS="-Wl,-rpath,/usr/local/lib "
PL_STATIC_LINK_LIBRARIES="-lpilot_light "
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_0.c "
PL_LINK_FRAMEWORKS="-framework Metal -framework MetalKit -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore "

# add flags for specific hardware
if [[ "$ARCH" == "arm64" ]]; then
    PL_COMPILER_FLAGS+="-arch arm64 "
else
    PL_COMPILER_FLAGS+="-arch x86_64 "
fi

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_0${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
clang -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES $PL_LINK_FRAMEWORKS -o "./../out/example_0.dylib"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_1 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out "
PL_COMPILER_FLAGS="-std=c99 --debug -g -fmodules -ObjC -fPIC "
PL_LINKER_FLAGS="-Wl,-rpath,/usr/local/lib "
PL_STATIC_LINK_LIBRARIES="-lpilot_light "
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_1.c "
PL_LINK_FRAMEWORKS="-framework Metal -framework MetalKit -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore "

# add flags for specific hardware
if [[ "$ARCH" == "arm64" ]]; then
    PL_COMPILER_FLAGS+="-arch arm64 "
else
    PL_COMPILER_FLAGS+="-arch x86_64 "
fi

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_1${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
clang -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES $PL_LINK_FRAMEWORKS -o "./../out/example_1.dylib"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_2 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out "
PL_COMPILER_FLAGS="-std=c99 --debug -g -fmodules -ObjC -fPIC "
PL_LINKER_FLAGS="-Wl,-rpath,/usr/local/lib "
PL_STATIC_LINK_LIBRARIES="-lpilot_light "
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_2.c "
PL_LINK_FRAMEWORKS="-framework Metal -framework MetalKit -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore "

# add flags for specific hardware
if [[ "$ARCH" == "arm64" ]]; then
    PL_COMPILER_FLAGS+="-arch arm64 "
else
    PL_COMPILER_FLAGS+="-arch x86_64 "
fi

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_2${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
clang -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES $PL_LINK_FRAMEWORKS -o "./../out/example_2.dylib"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_3 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out "
PL_COMPILER_FLAGS="-std=c99 --debug -g -fmodules -ObjC -fPIC "
PL_LINKER_FLAGS="-Wl,-rpath,/usr/local/lib "
PL_STATIC_LINK_LIBRARIES="-lpilot_light "
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_3.c "
PL_LINK_FRAMEWORKS="-framework Metal -framework MetalKit -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore "

# add flags for specific hardware
if [[ "$ARCH" == "arm64" ]]; then
    PL_COMPILER_FLAGS+="-arch arm64 "
else
    PL_COMPILER_FLAGS+="-arch x86_64 "
fi

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_3${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
clang -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES $PL_LINK_FRAMEWORKS -o "./../out/example_3.dylib"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_4 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out "
PL_COMPILER_FLAGS="-std=c99 --debug -g -fmodules -ObjC -fPIC "
PL_LINKER_FLAGS="-Wl,-rpath,/usr/local/lib "
PL_STATIC_LINK_LIBRARIES="-lpilot_light "
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_4.c "
PL_LINK_FRAMEWORKS="-framework Metal -framework MetalKit -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore "

# add flags for specific hardware
if [[ "$ARCH" == "arm64" ]]; then
    PL_COMPILER_FLAGS+="-arch arm64 "
else
    PL_COMPILER_FLAGS+="-arch x86_64 "
fi

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_4${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
clang -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES $PL_LINK_FRAMEWORKS -o "./../out/example_4.dylib"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_5 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out "
PL_COMPILER_FLAGS="-std=c99 --debug -g -fmodules -ObjC -fPIC "
PL_LINKER_FLAGS="-Wl,-rpath,/usr/local/lib "
PL_STATIC_LINK_LIBRARIES="-lpilot_light "
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_5.c "
PL_LINK_FRAMEWORKS="-framework Metal -framework MetalKit -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore "

# add flags for specific hardware
if [[ "$ARCH" == "arm64" ]]; then
    PL_COMPILER_FLAGS+="-arch arm64 "
else
    PL_COMPILER_FLAGS+="-arch x86_64 "
fi

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_5${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
clang -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES $PL_LINK_FRAMEWORKS -o "./../out/example_5.dylib"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_6 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out "
PL_COMPILER_FLAGS="-std=c99 --debug -g -fmodules -ObjC -fPIC "
PL_LINKER_FLAGS="-Wl,-rpath,/usr/local/lib "
PL_STATIC_LINK_LIBRARIES="-lpilot_light "
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_6.c "
PL_LINK_FRAMEWORKS="-framework Metal -framework MetalKit -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore "

# add flags for specific hardware
if [[ "$ARCH" == "arm64" ]]; then
    PL_COMPILER_FLAGS+="-arch arm64 "
else
    PL_COMPILER_FLAGS+="-arch x86_64 "
fi

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_6${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
clang -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES $PL_LINK_FRAMEWORKS -o "./../out/example_6.dylib"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_8 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out "
PL_COMPILER_FLAGS="-std=c99 --debug -g -fmodules -ObjC -fPIC "
PL_LINKER_FLAGS="-Wl,-rpath,/usr/local/lib "
PL_STATIC_LINK_LIBRARIES="-lpilot_light "
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_8.c "
PL_LINK_FRAMEWORKS="-framework Metal -framework MetalKit -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore "

# add flags for specific hardware
if [[ "$ARCH" == "arm64" ]]; then
    PL_COMPILER_FLAGS+="-arch arm64 "
else
    PL_COMPILER_FLAGS+="-arch x86_64 "
fi

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_8${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
clang -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES $PL_LINK_FRAMEWORKS -o "./../out/example_8.dylib"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

# delete lock file(s)
rm -f "../out/lock.tmp"

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# end of debug
fi


# return CWD to previous CWD
popd >/dev/null
