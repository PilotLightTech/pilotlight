#!/bin/bash

###############################################################################
#                                 Setup                                       #
###############################################################################

# colors
BOLD=$'\e[0;1m'
RED=$'\e[0;31m'
GREEN=$'\e[0;32m'
CYAN=$'\e[0;36m'
MAGENTA=$'\e[0;35m'
YELLOW=$'\e[0;33m'
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

# cleanup
if [ -f ../out/pilot_light ]; then
    rm -f ../out/pilot_light
    rm -f ../out/*.air
    rm -f ../out/*.metallib
    rm -f ../out/*.spv
fi

echo LOCKING > ../out/lock.tmp

###############################################################################
#                           Common Settings                                   #
###############################################################################

# build config: Debug or Release
PL_CONFIG=Debug

# common include directories
PL_INCLUDE_DIRECTORIES="-I../dependencies/stb"

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

###############################################################################
#                         Apple Common Settings                               #
###############################################################################

# common defines
PL_DEFINES="-D_DEBUG"

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
#                             apple pl lib                                    #
###############################################################################

PL_SOURCES="pl.c"

clang -c $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES -o ../out/pl.o

###############################################################################
#                             apple app lib                                   #
###############################################################################

PL_SOURCES="metal_app.m ../out/pl.o"

if [ -f "../out/app.so" ]
then
    rm ../out/app.so
fi

clang -shared -fPIC $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_LIBRARIES -o ../out/app.so

if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

###############################################################################
#                              apple executable                               #
###############################################################################

PL_SOURCES="apple_pl.m ../out/pl.o"

clang $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_LIBRARIES -o ../out/pilot_light

if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
elif [ -f "../out/app_0.so" ]
then
    rm ../out/app_*.so
fi

###############################################################################
#                            metal shaders                                    #
###############################################################################

# compile
# xcrun -sdk macosx metal -c ./shaders/simple.metal -o ../out/simple.air

# link
# xcrun -sdk macosx metallib ../out/*.air -o ../out/pl.metallib

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
PL_DEFINES="-D_DEBUG"

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
PL_COMPILER_FLAGS="-std=c99"

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
#                              linux pl lib                                   #
###############################################################################

PL_SOURCES="pl.c"

gcc -c -fPIC $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES -o ../out/pl.o

###############################################################################
#                             linux app lib                                   #
###############################################################################

PL_SOURCES="vulkan_app.c ../out/pl.o"

if [ -f "../out/app.so" ]
then
    rm ../out/app.so
fi

gcc -shared -fPIC $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_LIBRARIES -o ../out/app.so

if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

###############################################################################
#                              linux executable                               #
###############################################################################

PL_SOURCES="linux_pl.c ../out/pl.o"

gcc $PL_SOURCES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINK_FLAGS $PL_LINK_LIBRARIES -o ../out/pilot_light

if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
elif [ -f "../out/app_0.so" ]
then
    rm ../out/app_*.so
fi

###############################################################################
#                                vulkan shaders                               #
###############################################################################
# glslc -o ../out/simple.frag.spv ./shaders/simple.frag
# glslc -o ../out/simple.vert.spv ./shaders/simple.vert

fi

###############################################################################
#                          Information Output                                 #
###############################################################################
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