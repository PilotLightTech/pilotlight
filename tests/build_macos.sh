
# Auto Generated by:
# "pl_build.py" version: 1.0.12

# Project: pilotlight_tests

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
running_count=$(ps aux | grep -v grep | grep -ci "pilot_light_test")
if [ $running_count -gt 0 ]
then
    PL_HOT_RELOAD_STATUS=1
    echo
    echo ${BOLD}${WHITE}${RED_BG}--------${GREEN_BG} HOT RELOADING ${RED_BG}--------${NC}
    echo
else
    # cleanup binaries if not hot reloading
    PL_HOT_RELOAD_STATUS=0
    rm -f ../out/pilot_light_test

fi
#~~~~~~~~~~~~~~~~~~~~~~~~~~~ pilot_light_test | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~

# skip during hot reload
if [ $PL_HOT_RELOAD_STATUS -ne 1 ]; then

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-DPL_CONFIG_DEBUG "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out "
PL_COMPILER_FLAGS="-std=c99 --debug -g -fmodules -ObjC -fPIC "
PL_LINKER_FLAGS="-Wl,-rpath,/usr/local/lib "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="main_tests.c "
PL_LINK_FRAMEWORKS="-framework Metal -framework MetalKit -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore "

# add flags for specific hardware
if [[ "$ARCH" == "arm64" ]]; then
    PL_COMPILER_FLAGS+="-arch arm64 "
else
    PL_COMPILER_FLAGS+="-arch x86_64 "
fi

# run compiler (and linker)
echo
echo ${YELLOW}Step: pilot_light_test${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
clang $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/pilot_light_test"

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
rm -f ../out/lock.tmp

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# end of debug
fi

# ################################################################################
# #                           configuration | release                            #
# ################################################################################

if [[ "$PL_CONFIG" == "release" ]]; then

# create output directory(s)
mkdir -p "../out"

# create lock file(s)
echo LOCKING > "../out/lock.tmp"

# check if this is a reload
PL_HOT_RELOAD_STATUS=0

# # let user know if hot reloading
running_count=$(ps aux | grep -v grep | grep -ci "pilot_light_test")
if [ $running_count -gt 0 ]
then
    PL_HOT_RELOAD_STATUS=1
    echo
    echo ${BOLD}${WHITE}${RED_BG}--------${GREEN_BG} HOT RELOADING ${RED_BG}--------${NC}
    echo
else
    # cleanup binaries if not hot reloading
    PL_HOT_RELOAD_STATUS=0
    rm -f ../out/pilot_light_test

fi
#~~~~~~~~~~~~~~~~~~~~~~~~~~ pilot_light_test | release ~~~~~~~~~~~~~~~~~~~~~~~~~~

# skip during hot reload
if [ $PL_HOT_RELOAD_STATUS -ne 1 ]; then

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-DPL_CONFIG_RELEASE "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out "
PL_COMPILER_FLAGS="-std=c99 -fmodules -ObjC -fPIC "
PL_LINKER_FLAGS="-Wl,-rpath,/usr/local/lib "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="main_tests.c "
PL_LINK_FRAMEWORKS="-framework Metal -framework MetalKit -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore "

# add flags for specific hardware
if [[ "$ARCH" == "arm64" ]]; then
    PL_COMPILER_FLAGS+="-arch arm64 "
else
    PL_COMPILER_FLAGS+="-arch x86_64 "
fi

# run compiler (and linker)
echo
echo ${YELLOW}Step: pilot_light_test${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
clang $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/pilot_light_test"

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
rm -f ../out/lock.tmp

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# end of release
fi


# return CWD to previous CWD
popd >/dev/null
