#!/bin/bash

###############################################################################
#                                 Setup                                       #
###############################################################################

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

PL_RESULT=${BOLD}${GREEN}Successful.${NC}

# find directory of this script
SOURCE=${BASH_SOURCE[0]}
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
  SOURCE=$(readlink "$SOURCE")
  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

# make current directory the same as this script
pushd $DIR >/dev/null

# create output directory
if ! [[ -d ../out ]]; then
    mkdir ../out
fi

# get platform & architecture
PLAT="$(uname)"
ARCH="$(uname -m)"

echo LOCKING > ../out/lock.tmp
echo LOCKING > ../out/pl_draw_extension_lock.tmp

# check if hot reloading
PL_HOT_RELOADING_STATUS=0
if lsof | grep -i -q pilot_light
then
PL_HOT_RELOADING_STATUS=1
echo
echo ${BOLD}${WHITE}${RED_BG}--------${GREEN_BG} HOT RELOADING ${RED_BG}--------${NC}
echo
else
PL_HOT_RELOADING_STATUS=0
if [ -f ../out/pilot_light ]; then
    rm -f ../out/pl_draw_extension_*.so
    rm -f ../out/pilot_light
    rm -f ../out/*.spv
    rm ../out/app_*.so
fi
fi

###############################################################################
#                           Common Settings                                   #
###############################################################################

# build config: Debug or Release
PL_CONFIG=Debug

# common include directories
PL_INCLUDE_DIRECTORIES="-I../dependencies/stb -I../src -I../extensions"

# common link directories
PL_LINK_DIRECTORIES="-L../out"

###############################################################################
###############################################################################
###############################################################################
#                                Apple                                        #
###############################################################################
###############################################################################
###############################################################################

if [[ "$PLAT" == "Darwin" ]]; then

# cleanup
if [ -f ../out/pilot_light ]; then
    rm -f ../out/pilot_light
    rm -f ../out/*.air
    rm -f ../out/*.metallib
fi

###############################################################################
#                         Apple Common Settings                               #
###############################################################################

# common defines
PL_DEFINES="-D_DEBUG -DPL_METAL_BACKEND"

# common libraries & frameworks
PL_LINK_LIBRARIES="-framework Metal -framework MetalKit -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore"

# common compiler flags
PL_COMPILER_FLAGS="-std=c99 -ObjC -fmodules"

# arm64 specifics
if [[ "$ARCH" == "arm64" ]]; then

  # arm64 specific compiler flags
  PL_COMPILER_FLAGS+=" -arch arm64"

# x64 specifcis
else

  # x64 specific compiler flags
  PL_COMPILER_FLAGS+=" -arch x86_64"

fi

# debug specific
if [[ "$PL_CONFIG" == "Debug" ]]; then

  # debug specific compiler flags
  PL_COMPILER_FLAGS+=" --debug -g"

# release specific
elif [[ "$PL_CONFIG" == "Release" ]]; then
  echo "No release specifics yet"
fi

###############################################################################
#                            metal shaders                                    #
###############################################################################

echo
echo ${YELLOW}Step: shaders${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling...${NC}

# compile
# xcrun -sdk macosx metal -c ./shaders/simple.metal -o ../out/simple.air

# link
# xcrun -sdk macosx metallib ../out/*.air -o ../out/pl.metallib

###############################################################################
#                             apple pl lib                                    #
###############################################################################

PL_SOURCES="pilotlight.c"

echo
echo ${YELLOW}Step: pilotlight.o${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling...${NC}
clang -c $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES -o ../out/pilotlight.o

###############################################################################
#                             apple extensions                                #
###############################################################################

PL_SOURCES="../extensions/pl_draw_extension.c ../out/pilotlight.o"

if [ -f "../out/pl_draw_extension.so" ]
then
    rm ../out/pl_draw_extension.so
fi

echo
echo ${YELLOW}Step: app.so${NC}
echo ${YELLOW}~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling...${NC}
clang -shared -fPIC $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_LIBRARIES -o ../out/pl_draw_extension.so

if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

###############################################################################
#                             apple app lib                                   #
###############################################################################

PL_SOURCES="app_metal.m ../out/pilotlight.o"

if [ -f "../out/app.so" ]
then
    rm ../out/app.so
fi

echo
echo ${YELLOW}Step: app.so${NC}
echo ${YELLOW}~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling...${NC}
clang -shared -fPIC $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_LIBRARIES -o ../out/app.so

if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

###############################################################################
#                              apple executable                               #
###############################################################################

PL_SOURCES="pl_main_macos.m ../out/pilotlight.o"

if [ ${PL_HOT_RELOADING_STATUS} -ne 1 ]
then
echo
echo ${YELLOW}Step: pilot_light${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
clang $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_LIBRARIES -o ../out/pilot_light
fi

if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
elif [ -f "../out/app.so" ]
then
    rm ../out/app_*.so >/dev/null 2>&1
fi

###############################################################################
###############################################################################
###############################################################################
#                                Linux                                        #
###############################################################################
###############################################################################
###############################################################################
else

###############################################################################
#                           Linux Common Settings                             #
###############################################################################

# common defines
PL_DEFINES="-D_DEBUG -DPL_VULKAN_BACKEND"

# additional include directories
if [ -z "$VULKAN_SDK" ]
then
  echo "VULKAN_SDK env var not set"
else
  PL_INCLUDE_DIRECTORIES+=" -I$VULKAN_SDK/include"
  PL_LINK_DIRECTORIES+=" -L$VULKAN_SDK/lib"
fi

if [ -d /usr/include/vulkan ]; then
  PL_INCLUDE_DIRECTORIES+=" -I/usr/include/vulkan"
fi

# common libraries & frameworks
PL_LINK_LIBRARIES="-lvulkan -lxcb -lX11 -lX11-xcb -lxkbcommon"

# additional link directories
PL_LINK_DIRECTORIES+=" -L$VULKAN_SDK/lib"
PL_LINK_DIRECTORIES+=" -L/usr/lib/x86_64-linux-gnu"

# common compiler flags
PL_COMPILER_FLAGS="-std=gnu99"

# common linker flags
PL_LINK_FLAGS="-ldl -lm"

# debug specific
if [[ "$PL_CONFIG" == "Debug" ]]; then

  # debug specific compiler flags
  PL_COMPILER_FLAGS+=" --debug -g"

# release specific
elif [[ "$PL_CONFIG" == "Release" ]]; then
  echo "No release specifics yet"
fi

###############################################################################
#                                vulkan shaders                               #
###############################################################################
echo
echo ${YELLOW}Step 0: shaders${NC}
echo ${YELLOW}~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling...${NC}
# glslc -o ../out/simple.frag.spv ./shaders/simple.frag
# glslc -o ../out/simple.vert.spv ./shaders/simple.vert

###############################################################################
#                       linux pilotlight lib                                  #
###############################################################################

PL_SOURCES="pilotlight.c"

echo
echo ${YELLOW}Step: pilotlight.o${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling...${NC}
gcc -c -fPIC $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES -o ../out/pilotlight.o

###############################################################################
#                             linux extensions                                #
###############################################################################

PL_SOURCES="../extensions/pl_draw_extension.c ../out/pilotlight.o"

if [ -f "../out/pl_draw_extension.so" ]
then
    rm ../out/pl_draw_extension.so
fi

echo
echo ${YELLOW}Step: app.so${NC}
echo ${YELLOW}~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling...${NC}
gcc -shared -fPIC $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_LIBRARIES -o ../out/pl_draw_extension.so

if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

###############################################################################
#                             linux app lib                                   #
###############################################################################

PL_SOURCES="app_vulkan.c ../out/pilotlight.o"

if [ -f "../out/app.so" ]
then
    rm ../out/app.so
fi

echo
echo ${YELLOW}Step: app.so${NC}
echo ${YELLOW}~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling...${NC}
gcc -shared -fPIC $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_LIBRARIES -o ../out/app.so

if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

###############################################################################
#                              linux executable                               #
###############################################################################

PL_SOURCES="pl_main_linux.c ../out/pilotlight.o"

if [ ${PL_HOT_RELOADING_STATUS} -ne 1 ]
then
echo
echo ${YELLOW}Step: pilot_light${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINK_FLAGS $PL_LINK_LIBRARIES -o ../out/pilot_light
fi

if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

fi

###############################################################################
#                          Information Output                                 #
###############################################################################
echo
echo ${CYAN}-------------------------------------------------------------------------${NC}
echo ${YELLOW}                      Build Information ${NC}
echo ${CYAN}Results:             ${NC} ${PL_RESULT}
echo ${CYAN}Configuration:       ${NC} ${MAGENTA}${PL_CONFIG}${NC}
echo ${CYAN}Working directory:   ${NC} ${MAGENTA}${DIR}${NC}
echo ${CYAN}Output directory:    ${NC} ${MAGENTA}../out${NC}
echo ${CYAN}Output binary:       ${NC} ${YELLOW}pilot_light${NC}
echo ${CYAN}--------------------------------------------------------------------------${NC}

popd >/dev/null
rm ../out/lock.tmp
rm ../out/pl_draw_extension_lock.tmp