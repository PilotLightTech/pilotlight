#!/bin/bash

# Auto Generated by:
# "pl_build.py" version: 1.0.1

# Project: pilotlight

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
if pidof -x "pilot_light" -o $$ >/dev/null;then
    PL_HOT_RELOAD_STATUS=1
    echo
    echo echo ${BOLD}${WHITE}${RED_BG}--------${GREEN_BG} HOT RELOADING ${RED_BG}--------${NC}
    echo echo
else
    # cleanup binaries if not hot reloading
    echo PL_HOT_RELOAD_STATUS=0
    rm -f "../out/pilot_light.a"
    rm -f "../out/pilot_light.so"
    rm -f "../out/pilot_light_*.so"
    rm -f "../out/pl_script_camera.so"
    rm -f "../out/pl_script_camera_*.so"
    rm -f "../out/app.so"
    rm -f "../out/app_*.so"
    rm -f "../out/pilot_light"


fi
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ pl_lib | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# skip during hot reload
if [ $PL_HOT_RELOAD_STATUS -ne 1 ]; then

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS -D_DEBUG -DPL_VULKAN_BACKEND "
PL_INCLUDE_DIRECTORIES="-I../sandbox -I../src -I../libs -I../extensions -I../out -I../dependencies/stb -I../dependencies/cgltf "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES="-lxcb -lX11 -lX11-xcb -lxkbcommon -lxcb-cursor -lxcb-xfixes -lxcb-keysyms -lpthread "
# # run compiler only
echo
echo ${YELLOW}Step: pl_lib${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling...${NC}

# each file must be compiled separately
gcc -c $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS pl_lib.c -o "./../out/pl_lib.o"

# combine object files into a static lib
ar rcs ./../out/pilot_light.a ./../out/*.o
rm ./../out/*.o

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

# hot reload skip
fi

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ pl_ext | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS -D_DEBUG -DPL_VULKAN_BACKEND "
PL_INCLUDE_DIRECTORIES="-I../sandbox -I../src -I../libs -I../extensions -I../out -I../dependencies/stb -I../dependencies/cgltf -I$VULKAN_SDK/include -I/usr/include/vulkan "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu -L$VULKAN_SDK/lib "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES="-l:pilot_light.a "
PL_DYNAMIC_LINK_LIBRARIES="-lshaderc_shared -lxcb -lX11 -lX11-xcb -lxkbcommon -lxcb-cursor -lxcb-xfixes -lxcb-keysyms -lpthread -lvulkan "
PL_SOURCES="pl_ext.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: pl_ext${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/pilot_light.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~ pl_script_camera | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS -D_DEBUG -DPL_VULKAN_BACKEND "
PL_INCLUDE_DIRECTORIES="-I../sandbox -I../src -I../libs -I../extensions -I../out -I../dependencies/stb -I../dependencies/cgltf "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES="-l:pilot_light.a "
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="../extensions/pl_script_camera.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: pl_script_camera${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/pl_script_camera.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ app | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS -D_DEBUG -DPL_VULKAN_BACKEND "
PL_INCLUDE_DIRECTORIES="-I../sandbox -I../src -I../libs -I../extensions -I../out -I../dependencies/stb -I../dependencies/cgltf "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES="-l:pilot_light.a "
PL_DYNAMIC_LINK_LIBRARIES="-lxcb -lX11 -lX11-xcb -lxkbcommon -lxcb-cursor -lxcb-xfixes -lxcb-keysyms -lpthread "
PL_SOURCES="../sandbox/app.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: app${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/app.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ pilot_light | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# skip during hot reload
if [ $PL_HOT_RELOAD_STATUS -ne 1 ]; then

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS -D_DEBUG -DPL_VULKAN_BACKEND "
PL_INCLUDE_DIRECTORIES="-I../sandbox -I../src -I../libs -I../extensions -I../out -I../dependencies/stb -I../dependencies/cgltf "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES="-lxcb -lX11 -lX11-xcb -lxkbcommon -lxcb-cursor -lxcb-xfixes -lxcb-keysyms -lpthread "
PL_SOURCES="pl_main_x11.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: pilot_light${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/pilot_light"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

# hot reload skip
fi

# delete lock file(s)
rm -f "../out/lock.tmp"

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# end of debug
fi


# return CWD to previous CWD
popd >/dev/null
