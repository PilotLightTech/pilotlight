#!/bin/bash

# Auto Generated by:
# "pl_build.py" version: 1.0.12

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
    echo ${BOLD}${WHITE}${RED_BG}--------${GREEN_BG} HOT RELOADING ${RED_BG}--------${NC}
    echo
else
    # cleanup binaries if not hot reloading
    PL_HOT_RELOAD_STATUS=0
    rm -f ../out/example_0.so
    rm -f ../out/example_0_*.so
    rm -f ../out/example_1.so
    rm -f ../out/example_1_*.so
    rm -f ../out/example_2.so
    rm -f ../out/example_2_*.so
    rm -f ../out/example_3.so
    rm -f ../out/example_3_*.so
    rm -f ../out/example_4.so
    rm -f ../out/example_4_*.so
    rm -f ../out/example_5.so
    rm -f ../out/example_5_*.so
    rm -f ../out/example_6.so
    rm -f ../out/example_6_*.so
    rm -f ../out/example_8.so
    rm -f ../out/example_8_*.so
    rm -f ../out/example_9.so
    rm -f ../out/example_9_*.so


fi
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_0 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_0.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_0${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_0.so"

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
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_1.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_1${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_1.so"

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
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_2.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_2${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_2.so"

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
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_3.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_3${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_3.so"

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
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_4.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_4${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_4.so"

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
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_5.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_5${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_5.so"

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
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_6.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_6${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_6.so"

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
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_8.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_8${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_8.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_9 | debug ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC --debug -g "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_9.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_9${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_9.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

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
if pidof -x "pilot_light" -o $$ >/dev/null;then
    PL_HOT_RELOAD_STATUS=1
    echo
    echo ${BOLD}${WHITE}${RED_BG}--------${GREEN_BG} HOT RELOADING ${RED_BG}--------${NC}
    echo
else
    # cleanup binaries if not hot reloading
    PL_HOT_RELOAD_STATUS=0
    rm -f ../out/example_0.so
    rm -f ../out/example_0_*.so
    rm -f ../out/example_1.so
    rm -f ../out/example_1_*.so
    rm -f ../out/example_2.so
    rm -f ../out/example_2_*.so
    rm -f ../out/example_3.so
    rm -f ../out/example_3_*.so
    rm -f ../out/example_4.so
    rm -f ../out/example_4_*.so
    rm -f ../out/example_5.so
    rm -f ../out/example_5_*.so
    rm -f ../out/example_6.so
    rm -f ../out/example_6_*.so
    rm -f ../out/example_8.so
    rm -f ../out/example_8_*.so
    rm -f ../out/example_9.so
    rm -f ../out/example_9_*.so


fi
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_0 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_0.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_0${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_0.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_1 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_1.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_1${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_1.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_2 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_2.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_2${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_2.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_3 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_3.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_3${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_3.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_4 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_4.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_4${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_4.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_5 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_5.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_5${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_5.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_6 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_6.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_6${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_6.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_8 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_8.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_8${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_8.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ example_9 | release ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PL_RESULT=${BOLD}${GREEN}Successful.${NC}
PL_DEFINES="-D_USE_MATH_DEFINES -DPL_PROFILING_ON -DPL_ALLOW_HOT_RELOAD -DPL_ENABLE_VALIDATION_LAYERS "
PL_INCLUDE_DIRECTORIES="-I../examples -I../src -I../libs -I../extensions -I../out -I../dependencies/stb "
PL_LINK_DIRECTORIES="-L../out -L/usr/lib/x86_64-linux-gnu "
PL_COMPILER_FLAGS="-std=gnu11 -fPIC "
PL_LINKER_FLAGS="-ldl -lm "
PL_STATIC_LINK_LIBRARIES=""
PL_DYNAMIC_LINK_LIBRARIES=""
PL_SOURCES="example_9.c "

# run compiler (and linker)
echo
echo ${YELLOW}Step: example_9${NC}
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
gcc -shared $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_STATIC_LINK_LIBRARIES $PL_DYNAMIC_LINK_LIBRARIES -o "./../out/example_9.so"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}
echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}

# delete lock file(s)
rm -f ../out/lock.tmp

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# end of release
fi


# return CWD to previous CWD
popd >/dev/null
