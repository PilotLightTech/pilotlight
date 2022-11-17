__version__ = "0.1.0"

###############################################################################
#                                  Info                                       #
###############################################################################

# very poorly written & dirty system, to be cleaned up later

# TODO
#
# - error checking
# - testing
# - duplication catching
# - cleanup internals
# - handle multiple compilers per platform
# - combine multiple compilers/platforms in a single script
# - variable levels of verbosity in output scripts
# - add profiles maybe?
# - add shader support
# - pypi
# - documentation
# - platform independent flags (for common stuff)
# - generate .vscode stuff maybe

###############################################################################
#                                 Modules                                     #
###############################################################################

from enum import Enum
from contextlib import contextmanager
 
###############################################################################
#                                  Enums                                      #
###############################################################################


class FileType(Enum):
    UNKNOWN = 0
    BATCH = 1
    BASH = 2


class TargetType(Enum):
    NONE = 0
    STATIC_LIBRARY = 0
    DYNAMIC_LIBRARY = 1
    EXECUTABLE = 2


class CompilerType(Enum):
    NONE = 0
    MSVC = 1
    CLANG = 2
    GCC = 3


class PlatformType(Enum):
    NONE = 0
    WIN32 = 1
    MACOS = 2
    LINUX = 3


###############################################################################
#                                 Classes                                     #
###############################################################################


class CompilerSettings:
    def __init__(self, name: str, compiler_type: CompilerType):
        self._name = name
        self._compiler_type = compiler_type
        self._output_directory = "./"
        self._output_binary = ""
        self._output_binary_extension = None
        self._definitions = []
        self._compiler_flags = []
        self._linker_flags = []
        self._include_directories = []
        self._link_directories = []
        self._source_files = []
        self._link_libraries = []
        self._link_frameworks = []
        self._target_type = TargetType.NONE


class Platform:
    def __init__(self, platform_type: PlatformType):
        self._compiler_settings = []
        self._platform_type = platform_type


class CompilerConfiguration:
    def __init__(self, name: str):
        self._name = name
        self._platforms = []


class Target:
    def __init__(self, name: str, target_type: TargetType):
        self._name = name
        self._target_type = target_type
        self._lock_file = 'lock.tmp'
        self._configurations = []


class Project:
    def __init__(self, name: str):
        self._name = name
        self._win32_script_name = "build_" + name + "_win32"
        self._macos_script_name = "build_" + name + "_macos"
        self._linux_script_name = "build_" + name + "_linux"
        self._main_target_name = ""
        self._working_directory = "./"
        self._collapse = True # combine variables
        self._targets = []
        self._registered_options = []
        self._registered_flags = []
        self._registered_configurations = []


class BuildContext:
    def __init__(self):
        self._current_project = None
        self._current_target = None
        self._current_platform = None
        self._current_configuration = None
        self._current_compiler_settings = None
        self._projects = []

###############################################################################
#                             Global Context                                  #
###############################################################################
     
_context = BuildContext()

###############################################################################
#                                 Project                                     #
###############################################################################

@contextmanager
def project(name: str):
    try:
        _context._projects.append(Project(name))
        _context._current_project = _context._projects[-1]
        yield _context._current_project
    finally: 
        _context._current_project = None

def add_configuration(name: str):
    _context._current_project._registered_configurations.append(name)
    
def set_working_directory(directory: str):
    _context._current_project._working_directory = directory

def set_main_target(target_name: str):
    _context._current_project._main_target_name = target_name

def set_build_win32_script_name(script_name: str):
    _context._current_project._win32_script_name = script_name

def set_build_linux_script_name(script_name: str):
    _context._current_project._linux_script_name = script_name

def set_build_macos_script_name(script_name: str):
    _context._current_project._macos_script_name = script_name

###############################################################################
#                                 Target                                      #
###############################################################################

@contextmanager
def target(name: str, target_type: TargetType):
    try:
        _context._current_target = Target(name, target_type)
        yield _context._current_target
    finally:
        _context._current_project._targets.append(_context._current_target)
        _context._current_target = None

###############################################################################
#                             Configuration                                   #
###############################################################################

@contextmanager
def configuration(name: str):
    try:
        config = CompilerConfiguration(name)
        _context._current_configuration = config
        yield _context._current_configuration
    finally:
        _context._current_target._configurations.append(_context._current_configuration)
        _context._current_configuration = None

###############################################################################
#                                 Platform                                    #
###############################################################################

@contextmanager
def platform(platform_type: PlatformType):
    try:
        plat = Platform(platform_type)
        _context._current_platform = plat
        yield _context._current_platform
    finally:
        _context._current_configuration._platforms.append(_context._current_platform)
        _context._current_platform = None

###############################################################################
#                               Compiler                                      #
###############################################################################

@contextmanager
def compiler(name: str, compiler_type: CompilerType):
    try:
        compiler = CompilerSettings(name, compiler_type)
        _context._current_compiler_settings = compiler
        yield _context._current_compiler_settings
    finally:
        _context._current_platform._compiler_settings.append(_context._current_compiler_settings)
        _context._current_compiler_settings = None

def add_source_file(file: str):
    _context._current_compiler_settings._source_files.append(file)

def add_source_files(*args):
    for arg in args:
        add_source_file(arg)

def add_link_library(library: str):
    _context._current_compiler_settings._link_libraries.append(library)

def add_link_libraries(*args):
    for arg in args:
        add_link_library(arg)

def add_framework(library: str):
    _context._current_compiler_settings._link_frameworks.append(library)

def add_frameworks(*args):
    for arg in args:
        add_framework(arg)

def add_definition(definition: str):
    _context._current_compiler_settings._definitions.append(definition)

def add_definitions(*args):
    for arg in args:
        add_definition(arg)

def add_compiler_flag(flag: str):
    _context._current_compiler_settings._compiler_flags.append(flag)

def add_compiler_flags(*args):
    for arg in args:
        add_compiler_flag(arg)

def add_linker_flag(flag: str):
    _context._current_compiler_settings._linker_flags.append(flag)

def add_linker_flags(*args):
    for arg in args:
        add_linker_flag(arg)

def add_include_directory(directory: str):
    _context._current_compiler_settings._include_directories.append(directory)

def add_include_directories(*args):
    for arg in args:
        add_include_directory(arg)

def add_link_directory(directory: str):
    _context._current_compiler_settings._link_directories.append(directory)

def add_link_directories(*args):
    for arg in args:
        add_link_directory(arg)

def set_output_binary(binary: str):
    _context._current_compiler_settings._output_binary = binary

def set_output_binary_extension(extension: str):
    _context._current_compiler_settings._output_binary_extension = extension

def set_output_directory(directory: str):
    _context._current_compiler_settings._output_directory = directory

###############################################################################
#                               Generation                                    #
###############################################################################

def _title(title: str, file_type: FileType):

    line_length = 80
    padding = (line_length  - 2 - len(title)) / 2
    if file_type == FileType.BATCH:
        result = "@rem " + "#" * line_length + "\n"
        result += "@rem #" + " " * int(padding) + title + " " * int(padding + 0.5) + "#" + "\n"
        result += ("@rem " + "#" * line_length) + "\n"
    else:
        result = "#" * line_length + "\n"
        result += "#" + " " * int(padding) + title + " " * int(padding + 0.5) + "#" + "\n"
        result += "#" * line_length + "\n"
    return result

def _setup_defaults():

    for project in _context._projects:
        for target in project._targets:
            for config in target._configurations:
                for plat in config._platforms:
                    for settings in plat._compiler_settings:   
                        if settings._output_binary_extension is None:
                            if target._target_type == TargetType.STATIC_LIBRARY:
                                if settings._compiler_type == CompilerType.MSVC:
                                    settings._output_binary_extension = ".lib"
                                else :
                                    settings._output_binary_extension = ".o"
                            elif target._target_type == TargetType.DYNAMIC_LIBRARY:
                                if settings._compiler_type == CompilerType.MSVC:
                                    settings._output_binary_extension = ".dll"
                                else :
                                    settings._output_binary_extension = ".so"
                            elif target._target_type == TargetType.EXECUTABLE:
                                if settings._compiler_type == CompilerType.MSVC:
                                    settings._output_binary_extension = ".exe"
                                else :
                                    settings._output_binary_extension = ""

def generate_macos_build():

    _setup_defaults()

    for project in _context._projects:

        filepath = project._working_directory + "//" + project._macos_script_name + ".sh"
        file_type = FileType.BASH

        with open(filepath, "w") as file:
            buffer = "#!/bin/bash\n\n"

            buffer = "\n"
            buffer += '# Auto Generated by:\n'
            buffer += '# "pl_build.py" version: ' + __version__ + ' \n\n'

            buffer += _title("Development Setup", file_type)

            buffer += "\n# colors\n"
            buffer += "BOLD=$'\e[0;1m'\n"
            buffer += "RED=$'\e[0;31m'\n"
            buffer += "RED_BG=$'\e[0;41m'\n"
            buffer += "GREEN=$'\e[0;32m'\n"
            buffer += "GREEN_BG=$'\e[0;42m'\n"
            buffer += "CYAN=$'\e[0;36m'\n"
            buffer += "MAGENTA=$'\e[0;35m'\n"
            buffer += "YELLOW=$'\e[0;33m'\n"
            buffer += "WHITE=$'\e[0;97m'\n"
            buffer += "NC=$'\e[0m'\n\n"

            buffer += '# find directory of this script\n'
            buffer += "SOURCE=${BASH_SOURCE[0]}\n"
            buffer += 'while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink\n'
            buffer += '  DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )\n'
            buffer += '  SOURCE=$(readlink "$SOURCE")\n'
            buffer += '  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located\n'
            buffer += 'done\n'
            buffer += 'DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )\n\n'

            buffer += '# get architecture (intel or apple silicon)\n'
            buffer += 'ARCH="$(uname -m)"\n'

            buffer += '# make script directory CWD\n'
            buffer += 'pushd $DIR >/dev/null\n\n'

            if project._registered_configurations:
                buffer += '# default configuration\n'
                buffer += 'PL_CONFIG=' + project._registered_configurations[0] + '\n\n'
            
            buffer += '# check command line args for configuration\n'
            buffer += 'while getopts ":c:" option; do\n'
            buffer += '   case $option in\n'
            buffer += '   c) # set conf\n'
            buffer += '         PL_CONFIG=$OPTARG;;\n'
            buffer += '     \?) # Invalid option\n'
            buffer += '         echo "Error: Invalid option"\n'
            buffer += '         exit;;\n'
            buffer += '   esac\n'
            buffer += 'done\n\n'

            for register_config in project._registered_configurations:

                # find main target
                target_found = False
                for target in project._targets:
                    if target._target_type == TargetType.EXECUTABLE and target._name == project._main_target_name:
                        for config in target._configurations:
                            if config._name == register_config:
                                for plat in config._platforms:
                                    if plat._platform_type == PlatformType.MACOS:
                                        for settings in plat._compiler_settings:
                                            if settings._compiler_type == CompilerType.CLANG:
                                                
                                                buffer += _title("configuration | " + register_config, file_type)

                                                buffer += '\nif [[ "$PL_CONFIG" == "' + register_config + '" ]]; then\n\n'
                                                
                                                buffer += '# check if this is a reload\n'
                                                buffer += 'PL_HOT_RELOAD_STATUS=0\n\n'

                                                buffer += "# let user know if hot reloading\n"
                                                buffer += 'if lsof | grep -i -q ' + settings._output_binary + '\n'
                                                buffer += 'then\n'
                                                buffer += 'PL_HOT_RELOAD_STATUS=1\n'
                                                buffer += 'echo\n'
                                                buffer += 'echo ${BOLD}${WHITE}${RED_BG}--------${GREEN_BG} HOT RELOADING ${RED_BG}--------${NC}\n'
                                                buffer += 'echo\n'
                                                buffer += 'else\n'
                                                buffer += '# cleanup binaries if not hot reloading\n'
                                                buffer += '    PL_HOT_RELOAD_STATUS=0\n'
                                                target_found = True
                for target in project._targets:
                    for config in target._configurations:
                        if config._name == register_config:
                            for plat in config._platforms:
                                if plat._platform_type == PlatformType.MACOS:
                                    for settings in plat._compiler_settings:
                                        if settings._compiler_type == CompilerType.CLANG:
                                            if target._target_type == TargetType.EXECUTABLE:
                                                buffer += '    rm -f ./' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension + '\n'
                                            elif target._target_type == TargetType.DYNAMIC_LIBRARY:
                                                buffer += '    rm -f ./' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension + '\n'
                                                buffer += '    rm -f ./' + settings._output_directory + '/' + settings._output_binary + '_*' + settings._output_binary_extension + '\n'
                                            elif target._target_type == TargetType.STATIC_LIBRARY:
                                                buffer += '    rm -f ./' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension + '\n'
                if target_found:
                    buffer += "fi\n"
                buffer += "\n"
                for target in project._targets:
                    for config in target._configurations:
                        if config._name == register_config:
                            for plat in config._platforms:
                                if plat._platform_type == PlatformType.MACOS:
                                    for settings in plat._compiler_settings:
                                        if settings._compiler_type == CompilerType.CLANG:

                                            buffer += _title(config._name + " | " + target._name, file_type)
                                            buffer += '\n# create output directory\n'
                                            buffer += 'if ! [[ -d "' + settings._output_directory + '" ]]; then\n'
                                            buffer += '    mkdir "' + settings._output_directory + '"\n'
                                            buffer += 'fi\n\n'

                                            buffer += '# create lock file\n'
                                            buffer += 'echo LOCKING > "./' + settings._output_directory + '/' + target._lock_file + '"\n\n'

                                            buffer += '# preprocessor defines\n'
                                            if project._collapse:
                                                buffer += 'PL_DEFINES="'    
                                                for define in settings._definitions:
                                                    buffer += '-D' + define + " "
                                                buffer += '"\n'
                                            else:
                                                buffer += 'PL_DEFINES=\n'    
                                                for define in settings._definitions:
                                                    buffer += 'PL_DEFINES+=" -D' + define + '"\n'
                                            buffer += '\n'

                                            buffer += '# includes directories\n'
                                            if project._collapse:
                                                buffer += 'PL_INCLUDE_DIRECTORIES="'    
                                                for include in settings._include_directories:
                                                    buffer += '-I' + include + ' '
                                                buffer += '"\n'
                                            else:
                                                buffer += 'PL_INCLUDE_DIRECTORIES=\n'    
                                                for include in settings._include_directories:
                                                    buffer += 'PL_INCLUDE_DIRECTORIES+=" -I' + include + '"\n'
                                            buffer += '\n'

                                            buffer += '# link directories\n'
                                            if project._collapse:
                                                buffer += 'PL_LINK_DIRECTORIES="'    
                                                for link in settings._link_directories:
                                                    buffer += '-L' + link + ' '
                                                buffer += '"\n'
                                            else:
                                                buffer += 'PL_LINK_DIRECTORIES=\n'    
                                                for link in settings._link_directories:
                                                    buffer += 'PL_LINK_DIRECTORIES+=" -L' + link + '"\n'
                                            buffer += '\n'

                                            buffer += '# compiler flags\n'
                                            if project._collapse:
                                                buffer += 'PL_COMPILER_FLAGS="'    
                                                for flag in settings._compiler_flags:
                                                    buffer += flag + ' '
                                                buffer += '"\n'
                                            else:
                                                buffer += 'PL_COMPILER_FLAGS=\n'    
                                                for flag in settings._compiler_flags:
                                                    buffer += 'PL_COMPILER_FLAGS+=" ' + flag + '"\n'

                                            buffer += '\n# add flags for specific hardware\n'
                                            buffer += 'if [[ "$ARCH" == "arm64" ]]; then\n'
                                            buffer += '    PL_COMPILER_FLAGS+="-arch arm64 "\n'
                                            buffer += 'else\n'
                                            buffer += '    PL_COMPILER_FLAGS+="-arch x86_64 "\n'
                                            buffer += 'fi\n'
                                            buffer += "\n"

                                            buffer += '# linker flags\n'
                                            if project._collapse:
                                                buffer += 'PL_LINKER_FLAGS="'    
                                                for flag in settings._linker_flags:
                                                    buffer += '-l' + flag + ' '
                                                buffer += '"\n'
                                            else:
                                                buffer += 'PL_LINKER_FLAGS=\n'    
                                                for flag in settings._linker_flags:
                                                    buffer += 'PL_LINKER_FLAGS+=" -l' + flag + '"\n'
                                            buffer += '\n'

                                            buffer += '# libraries\n'
                                            if project._collapse:
                                                buffer += 'PL_LINK_LIBRARIES="'    
                                                for link in settings._link_libraries:
                                                    buffer += '-l ' + link + ' '
                                                buffer += '"\n'
                                            else:
                                                buffer += 'PL_LINK_LIBRARIES=\n'    
                                                for link in settings._link_libraries:
                                                    buffer += 'PL_LINK_LIBRARIES+=" -l ' + link + '"\n'
                                            buffer += '\n'

                                            buffer += '# frameworks\n'
                                            if project._collapse:
                                                buffer += 'PL_LINK_FRAMEWORKS="'    
                                                for link in settings._link_frameworks:
                                                    buffer += '-framework ' + link + ' '
                                                buffer += '"\n'
                                            else:
                                                buffer += 'PL_LINK_FRAMEWORKS=\n'    
                                                for link in settings._link_frameworks:
                                                    buffer += 'PL_LINK_FRAMEWORKS+=" -framework ' + link + '"\n'
                                            buffer += '\n'

                                            buffer += "# default compilation result\n"
                                            buffer += "PL_RESULT=${BOLD}${GREEN}Successful.${NC}\n\n"

                                            if target._target_type == TargetType.STATIC_LIBRARY:
                
                                                buffer += '# run compiler only\n'
                                                buffer += "echo\n"
                                                buffer += 'echo ${YELLOW}Step: ' + target._name +'${NC}\n'
                                                buffer += 'echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}\n'
                                                buffer += 'echo ${CYAN}Compiling...${NC}\n'
                                            
                                                buffer += '\n# each file must be compiled separately\n'
                                                for source in settings._source_files:
                                                    buffer += 'clang -c -fPIC $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS ' + source + ' -o "./' + settings._output_directory + '/' + source + '.o"\n'

                                            elif target._target_type == TargetType.DYNAMIC_LIBRARY:

                                                buffer += '# source files\n'
                                                if project._collapse:
                                                    buffer += 'PL_SOURCES="'    
                                                    for source in settings._source_files:
                                                        buffer += source + ' '
                                                    buffer += '"\n'
                                                else:
                                                    buffer += 'PL_SOURCES=\n'    
                                                    for source in settings._source_files:
                                                        buffer += 'PL_SOURCES+=" ' + source + '"\n'

                                                buffer += '\n# run compiler (and linker)\n'
                                                buffer += "echo\n"
                                                buffer += 'echo ${YELLOW}Step: ' + target._name +'${NC}\n'
                                                buffer += 'echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}\n'
                                                buffer += 'echo ${CYAN}Compiling and Linking...${NC}\n'
                                                buffer += 'clang -shared -fPIC $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_LINK_LIBRARIES $PL_LINK_FRAMEWORKS -o "./' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension +'"\n'

                                            elif target._target_type == TargetType.EXECUTABLE:

                                                buffer += '# source files\n'
                                                if project._collapse:
                                                    buffer += 'PL_SOURCES="'    
                                                    for source in settings._source_files:
                                                        buffer += source + ' '
                                                    buffer += '"\n'
                                                else:
                                                    buffer += 'PL_SOURCES=\n'    
                                                    for source in settings._source_files:
                                                        buffer += 'PL_SOURCES+=" ' + source + '"\n'

                                                buffer += '\n# run compiler (and linker)\n'
                                                buffer += "echo\n"
                                                buffer += 'echo ${YELLOW}Step: ' + target._name +'${NC}\n'
                                                buffer += 'echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}\n'
                                                buffer += 'echo ${CYAN}Compiling and Linking...${NC}\n'
                                                buffer += 'clang -fPIC $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_LINK_LIBRARIES -o "./' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension +'"\n'
                
                                            buffer += "\n# check build status\n"
                                            buffer += "if [ $? -ne 0 ]\n"
                                            buffer += "then\n"
                                            buffer += "    PL_RESULT=${BOLD}${RED}Failed.${NC}\n"
                                            buffer += "fi\n"

                                            buffer += "\n# print results\n"
                                            buffer += "echo ${CYAN}Results: ${NC} ${PL_RESULT}\n"
                                            buffer += "echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}\n"

                                            buffer += '\n# remove lock file\n'
                                            buffer += 'rm "./' + settings._output_directory + '/' + target._lock_file + '"\n\n'

                if target_found:
                    buffer += '#' + '~' * 40 + '\n'
                    buffer += '# end of ' + register_config + ' \n'
                    buffer += 'fi\n'
            buffer += '# return CWD to previous CWD\n'
            buffer += 'popd >/dev/null'
            file.write(buffer)

def generate_linux_build():

    _setup_defaults()

    for project in _context._projects:

        filepath = project._working_directory + "//" + project._linux_script_name + ".sh"
        file_type = FileType.BASH

        with open(filepath, "w") as file:
            buffer = "#!/bin/bash\n\n"
            buffer += '# Auto Generated by:\n'
            buffer += '# "pl_build.py" version: ' + __version__ + ' \n\n'

            buffer += _title("Development Setup", file_type)

            buffer += "\n# colors\n"
            buffer += "BOLD=$'\e[0;1m'\n"
            buffer += "RED=$'\e[0;31m'\n"
            buffer += "RED_BG=$'\e[0;41m'\n"
            buffer += "GREEN=$'\e[0;32m'\n"
            buffer += "GREEN_BG=$'\e[0;42m'\n"
            buffer += "CYAN=$'\e[0;36m'\n"
            buffer += "MAGENTA=$'\e[0;35m'\n"
            buffer += "YELLOW=$'\e[0;33m'\n"
            buffer += "WHITE=$'\e[0;97m'\n"
            buffer += "NC=$'\e[0m'\n\n"

            buffer += '# find directory of this script\n'
            buffer += "SOURCE=${BASH_SOURCE[0]}\n"
            buffer += 'while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink\n'
            buffer += '  DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )\n'
            buffer += '  SOURCE=$(readlink "$SOURCE")\n'
            buffer += '  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located\n'
            buffer += 'done\n'
            buffer += 'DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )\n\n'

            buffer += '# make script directory CWD\n'
            buffer += 'pushd $DIR >/dev/null\n\n'

            if project._registered_configurations:
                buffer += '# default configuration\n'
                buffer += 'PL_CONFIG=' + project._registered_configurations[0] + '\n\n'
            
            buffer += '# check command line args for configuration\n'
            buffer += 'while getopts ":c:" option; do\n'
            buffer += '   case $option in\n'
            buffer += '   c) # set conf\n'
            buffer += '         PL_CONFIG=$OPTARG;;\n'
            buffer += '     \?) # Invalid option\n'
            buffer += '         echo "Error: Invalid option"\n'
            buffer += '         exit;;\n'
            buffer += '   esac\n'
            buffer += 'done\n\n'

            for register_config in project._registered_configurations:
                
                # find main target
                target_found = False        
                for target in project._targets:
                    if target._target_type == TargetType.EXECUTABLE and target._name == project._main_target_name:
                        for config in target._configurations:
                            if config._name == register_config:
                                for plat in config._platforms:
                                    if plat._platform_type == PlatformType.LINUX:
                                        for settings in plat._compiler_settings:
                                            if settings._compiler_type == CompilerType.GCC:

                                                buffer += _title("configuration | " + register_config, file_type)

                                                buffer += '\nif [[ "$PL_CONFIG" == "' + register_config + '" ]]; then\n\n'
                                                
                                                buffer += '# check if this is a reload\n'
                                                buffer += 'PL_HOT_RELOAD_STATUS=0\n\n'

                                                buffer += "# let user know if hot reloading\n"
                                                buffer += 'if lsof | grep -i -q ' + settings._output_binary + '\n'
                                                buffer += 'then\n'
                                                buffer += 'PL_HOT_RELOAD_STATUS=1\n'
                                                buffer += 'echo\n'
                                                buffer += 'echo ${BOLD}${WHITE}${RED_BG}--------${GREEN_BG} HOT RELOADING ${RED_BG}--------${NC}\n'
                                                buffer += 'echo\n'
                                                buffer += 'else\n'
                                                buffer += '# cleanup binaries if not hot reloading\n'
                                                buffer += '    PL_HOT_RELOAD_STATUS=0\n'
                                                target_found = True
                for target in project._targets:
                    for config in target._configurations:
                        if config._name == register_config:
                            for plat in config._platforms:
                                if plat._platform_type == PlatformType.LINUX:
                                    for settings in plat._compiler_settings:
                                        if settings._compiler_type == CompilerType.GCC:
                                            if target._target_type == TargetType.EXECUTABLE:
                                                buffer += '    rm -f ./' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension + '\n'
                                            elif target._target_type == TargetType.DYNAMIC_LIBRARY:
                                                buffer += '    rm -f ./' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension + '\n'
                                                buffer += '    rm -f ./' + settings._output_directory + '/' + settings._output_binary + '_*' + settings._output_binary_extension + '\n'
                                            elif target._target_type == TargetType.STATIC_LIBRARY:
                                                buffer += '    rm -f ./' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension + '\n'
                if target_found:
                    buffer += "fi\n"
                buffer += "\n"
                for target in project._targets:
                    for config in target._configurations:
                        if config._name == register_config:
                            for plat in config._platforms:
                                if plat._platform_type == PlatformType.LINUX:
                                    for settings in plat._compiler_settings:
                                        if settings._compiler_type == CompilerType.GCC:

                                            buffer += _title(config._name + " | " + target._name, file_type)
                                            buffer += '\n# create output directory\n'
                                            buffer += 'if ! [[ -d "' + settings._output_directory + '" ]]; then\n'
                                            buffer += '    mkdir "' + settings._output_directory + '"\n'
                                            buffer += 'fi\n\n'

                                            buffer += '# create lock file\n'
                                            buffer += 'echo LOCKING > "./' + settings._output_directory + '/' + target._lock_file + '"\n\n'

                                            buffer += '# preprocessor defines\n'
                                            if project._collapse:
                                                buffer += 'PL_DEFINES="'    
                                                for define in settings._definitions:
                                                    buffer += '-D' + define + " "
                                                buffer += '"\n'
                                            else:
                                                buffer += 'PL_DEFINES=\n'    
                                                for define in settings._definitions:
                                                    buffer += 'PL_DEFINES+=" -D' + define + '"\n'
                                            buffer += '\n'

                                            buffer += '# includes directories\n'
                                            if project._collapse:
                                                buffer += 'PL_INCLUDE_DIRECTORIES="'    
                                                for include in settings._include_directories:
                                                    buffer += '-I' + include + ' '
                                                buffer += '"\n'
                                            else:
                                                buffer += 'PL_INCLUDE_DIRECTORIES=\n'    
                                                for include in settings._include_directories:
                                                    buffer += 'PL_INCLUDE_DIRECTORIES+=" -I' + include + '"\n'
                                            buffer += '\n'

                                            buffer += '# link directories\n'
                                            if project._collapse:
                                                buffer += 'PL_LINK_DIRECTORIES="'    
                                                for link in settings._link_directories:
                                                    buffer += '-L' + link + ' '
                                                buffer += '"\n'
                                            else:
                                                buffer += 'PL_LINK_DIRECTORIES=\n'    
                                                for link in settings._link_directories:
                                                    buffer += 'PL_LINK_DIRECTORIES+=" -L' + link + '"\n'
                                            buffer += '\n'

                                            buffer += '# compiler flags\n'
                                            if project._collapse:
                                                buffer += 'PL_COMPILER_FLAGS="'    
                                                for flag in settings._compiler_flags:
                                                    buffer += flag + ' '
                                                buffer += '"\n'
                                            else:
                                                buffer += 'PL_COMPILER_FLAGS=\n'    
                                                for flag in settings._compiler_flags:
                                                    buffer += 'PL_COMPILER_FLAGS+=" ' + flag + '"\n'
                                            buffer += '\n'

                                            buffer += '# linker flags\n'
                                            if project._collapse:
                                                buffer += 'PL_LINKER_FLAGS="'    
                                                for flag in settings._linker_flags:
                                                    buffer += '-l' + flag + ' '
                                                buffer += '"\n'
                                            else:
                                                buffer += 'PL_LINKER_FLAGS=\n'    
                                                for flag in settings._linker_flags:
                                                    buffer += 'PL_LINKER_FLAGS+=" -l' + flag + '"\n'
                                            buffer += '\n'

                                            buffer += '# libraries\n'
                                            if project._collapse:
                                                buffer += 'PL_LINK_LIBRARIES="'    
                                                for link in settings._link_libraries:
                                                    buffer += '-l' + link + ' '
                                                buffer += '"\n'
                                            else:
                                                buffer += 'PL_LINK_LIBRARIES=\n'    
                                                for link in settings._link_libraries:
                                                    buffer += 'PL_LINK_LIBRARIES+=" -l' + link + '"\n'
                                            buffer += '\n'


                                            buffer += "# default compilation result\n"
                                            buffer += "PL_RESULT=${BOLD}${GREEN}Successful.${NC}\n\n"
                                            
                                            if target._target_type == TargetType.STATIC_LIBRARY:
                
                                                buffer += '# run compiler only\n'
                                                buffer += "echo\n"
                                                buffer += 'echo ${YELLOW}Step: ' + target._name +'${NC}\n'
                                                buffer += 'echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}\n'
                                                buffer += 'echo ${CYAN}Compiling...${NC}\n'
                                            
                                                buffer += '\n# each file must be compiled separately\n'
                                                for source in settings._source_files:
                                                    buffer += 'gcc -c -fPIC $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS ' + source + ' -o "./' + settings._output_directory + '/' + source + '.o"\n'
                                                
                                            elif target._target_type == TargetType.DYNAMIC_LIBRARY:

                                                buffer += '# source files\n'
                                                if project._collapse:
                                                    buffer += 'PL_SOURCES="'    
                                                    for source in settings._source_files:
                                                        buffer += source + ' '
                                                    buffer += '"\n'
                                                else:
                                                    buffer += 'PL_SOURCES=\n'    
                                                    for source in settings._source_files:
                                                        buffer += 'PL_SOURCES+=" ' + source + '"\n'

                                                buffer += '\n# run compiler (and linker)\n'
                                                buffer += "echo\n"
                                                buffer += 'echo ${YELLOW}Step: ' + target._name +'${NC}\n'
                                                buffer += 'echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}\n'
                                                buffer += 'echo ${CYAN}Compiling and Linking...${NC}\n'
                                                buffer += 'gcc -shared -fPIC $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_LINK_LIBRARIES -o "./' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension +'"\n'

                                            elif target._target_type == TargetType.EXECUTABLE:

                                                buffer += 'PL_SOURCES=\n'    
                                                for source in settings._source_files:
                                                    buffer += 'PL_SOURCES+=" ' + source + '"\n'

                                                buffer += '\n# run compiler (and linker)\n'
                                                buffer += "echo\n"
                                                buffer += 'echo ${YELLOW}Step: ' + target._name +'${NC}\n'
                                                buffer += 'echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}\n'
                                                buffer += 'echo ${CYAN}Compiling and Linking...${NC}\n'
                                                buffer += 'gcc -fPIC $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_LINK_LIBRARIES -o "./' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension +'"\n'

                                            buffer += "\n# check build status\n"
                                            buffer += "if [ $? -ne 0 ]\n"
                                            buffer += "then\n"
                                            buffer += "    PL_RESULT=${BOLD}${RED}Failed.${NC}\n"
                                            buffer += "fi\n"

                                            buffer += "\n# print results\n"
                                            buffer += "echo ${CYAN}Results: ${NC} ${PL_RESULT}\n"
                                            buffer += "echo ${CYAN}~~~~~~~~~~~~~~~~~~~~~~${NC}\n"

                                            buffer += '\n# remove lock file\n'
                                            buffer += 'rm "./' + settings._output_directory + '/' + target._lock_file + '"\n\n'
                if target_found:
                    buffer += '#' + '~' * 40 + '\n'
                    buffer += '# end of ' + register_config + ' \n'
                    buffer += 'fi\n'
            buffer += '# return CWD to previous CWD\n'
            buffer += 'popd >/dev/null'
            file.write(buffer)

def generate_win32_build():

    _setup_defaults()

    for project in _context._projects:

        filepath = project._working_directory + "/" + project._win32_script_name + ".bat"
        file_type = FileType.BATCH

        with open(filepath, "w") as file:
            buffer = "\n"
            buffer += '@rem Auto Generated by:\n'
            buffer += '@rem "pl_build.py" version: ' + __version__ + ' \n\n'

            buffer += _title("Development Setup", file_type)

            buffer += '\n@rem keep environment variables modifications local\n'
            buffer += '@setlocal\n\n'

            buffer += '@rem make script directory CWD\n'
            buffer += '@pushd %~dp0\n'
            buffer += '@set dir=%~dp0\n\n'

            buffer += '@rem modify PATH to find vcvarsall.bat\n'
            buffer += '@set PATH=C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build;%PATH%\n'
            buffer += '@set PATH=C:\\Program Files\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build;%PATH%\n'
            buffer += '@set PATH=C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build;%PATH%\n'
            buffer += '@set PATH=C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build;%PATH%\n'
            buffer += '@set PATH=C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise/VC\\Auxiliary\\Build;%PATH%\n\n'

            buffer += '@rem setup environment for MSVC dev tools\n'
            buffer += '@call vcvarsall.bat amd64 > nul\n\n'

            buffer += '@rem default compilation result\n'
            buffer += '@set PL_RESULT=[1m[92mSuccessful.[0m\n\n'

            if project._registered_configurations:
                buffer += '@rem default configuration\n'
                buffer += "@set PL_CONFIG=" + project._registered_configurations[0] + "\n\n"
            
            buffer += '@rem check command line args for configuration\n'
            buffer += ":CheckConfiguration\n"
            buffer += '@if "%~1"=="-c" (@set PL_CONFIG=%2) & @shift & @shift & @goto CheckConfiguration\n'
            for register_config in project._registered_configurations:
                buffer += '@if "%PL_CONFIG%" equ "' + register_config +  '" ( goto ' + register_config + ' )\n'
            buffer += "\n"


            for register_config in project._registered_configurations:

                # find main target
                target_found = False
                for target in project._targets:
                    if target._target_type == TargetType.EXECUTABLE and target._name == project._main_target_name:
                        for config in target._configurations:
                            if config._name == register_config:
                                for plat in config._platforms:
                                    if plat._platform_type == PlatformType.WIN32:
                                        for settings in plat._compiler_settings:
                                            if settings._compiler_type == CompilerType.MSVC:

                                                buffer += _title("configuration | " + register_config, file_type)
                                                buffer += "\n:" + register_config + "\n\n"

                                                buffer += "@rem create main target output directoy\n"
                                                buffer += '@if not exist "' + settings._output_directory + '" @mkdir "' + settings._output_directory + '"\n\n'

                                                buffer += "@rem check if this is a reload\n"
                                                buffer += "@set PL_HOT_RELOAD_STATUS=0\n"
                                                
                                                buffer += "\n@rem hack to see if main exe is running\n"
                                                buffer += "@echo off\n"
                                                buffer += '2>nul (>>"' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension + '" echo off) && (@set PL_HOT_RELOAD_STATUS=0) || (@set PL_HOT_RELOAD_STATUS=1)\n'
                                                
                                                buffer += "\n@rem let user know if hot reloading\n"
                                                buffer += "@if %PL_HOT_RELOAD_STATUS% equ 1 (\n"
                                                buffer += "    @echo.\n"
                                                buffer += "    @echo [1m[97m[41m--------[42m HOT RELOADING [41m--------[0m\n"
                                                buffer += ")\n\n"
                                                buffer += "@rem cleanup binaries if not hot reloading\n"
                                                buffer += "@if %PL_HOT_RELOAD_STATUS% equ 0 (\n    @echo.\n"
                                                target_found = True
                
                for target in project._targets:
                    for config in target._configurations:
                        if config._name == register_config:
                            for plat in config._platforms:
                                if plat._platform_type == PlatformType.WIN32:
                                    for settings in plat._compiler_settings:
                                        if settings._compiler_type == CompilerType.MSVC:
                                            buffer += '    @if exist "' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension + '"'
                                            buffer += ' del "' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension + '"\n'
                                        if target._target_type == TargetType.DYNAMIC_LIBRARY:
                                            buffer += '    @if exist "' + settings._output_directory + '/' + settings._output_binary + '_*' + settings._output_binary_extension + '"'
                                            buffer += ' del "' + settings._output_directory + '/' + settings._output_binary + '_*' + settings._output_binary_extension + '"\n'
                                            buffer += '    @if exist "' + settings._output_directory + '/' + settings._output_binary + '_*.pdb"'
                                            buffer += ' del "' + settings._output_directory + '/' + settings._output_binary + '_*.pdb"\n'
                if target_found:
                    buffer += ")"
                buffer += "\n\n"
                for target in project._targets:
                    for config in target._configurations:
                        if config._name == register_config:

                            buffer += _title(config._name + " | " + target._name, file_type)
                            buffer += "\n"

                            for plat in config._platforms:
                                if plat._platform_type == PlatformType.WIN32:
                                    for settings in plat._compiler_settings:

                                        if settings._compiler_type == CompilerType.MSVC:

                                            buffer += '@rem create output directory\n'
                                            buffer += '@if not exist "' + settings._output_directory + '" @mkdir "' + settings._output_directory + '"'
                                            buffer += "\n\n"

                                            buffer += '@rem create lock file\n'
                                            buffer += '@echo LOCKING > "' + settings._output_directory + '/' + target._lock_file + '"'
                                            buffer += "\n\n"
                                            
                                            if settings._definitions:
                                                buffer += '@rem preprocessor defines\n'
                                                if project._collapse:
                                                    buffer += '@set PL_DEFINES='
                                                    for define in settings._definitions:
                                                        buffer += "-D" + define + " "
                                                else:
                                                    buffer += '@set PL_DEFINES=\n'
                                                    for define in settings._definitions:
                                                        buffer += '@set PL_DEFINES=-D' + define + " %PL_DEFINES%\n"               
                                                buffer += "\n\n"

                                            if settings._include_directories:
                                                buffer += '@rem include directories\n'
                                                if project._collapse:
                                                    buffer += '@set PL_INCLUDE_DIRECTORIES='    
                                                    for include in settings._include_directories:
                                                        buffer += '-I"' + include + '" '
                                                else:
                                                    buffer += '@set PL_INCLUDE_DIRECTORIES=\n'    
                                                    for include in settings._include_directories:
                                                        buffer += '@set PL_INCLUDE_DIRECTORIES=-I"' + include + '" %PL_INCLUDE_DIRECTORIES%\n'
                                                buffer += "\n\n"

                                            if settings._link_directories:
                                                buffer += '@rem link directories\n'
                                                if project._collapse:
                                                    buffer += '@set PL_LINK_DIRECTORIES='    
                                                    for link in settings._link_directories:
                                                        buffer += '-LIBPATH:"' + link + '" '
                                                else:
                                                    buffer += '@set PL_LINK_DIRECTORIES=\n'    
                                                    for link in settings._link_directories:
                                                        buffer += '@set PL_LINK_DIRECTORIES=-LIBPATH:"' + link + '" %PL_LINK_DIRECTORIES%\n'
                                                buffer += "\n\n"

                                            if settings._compiler_flags:
                                                buffer += '@rem compiler flags\n'
                                                if project._collapse:
                                                    buffer += '@set PL_COMPILER_FLAGS='    
                                                    for flag in settings._compiler_flags:
                                                        buffer += flag + " "
                                                else:
                                                    buffer += '@set PL_COMPILER_FLAGS=\n'    
                                                    for flag in settings._compiler_flags:
                                                        buffer += '@set PL_COMPILER_FLAGS=' + flag + " %PL_COMPILER_FLAGS%\n"
                                                buffer += "\n\n"

                                            if settings._linker_flags:
                                                buffer += '@rem linker flags\n'
                                                if project._collapse:
                                                    buffer += '@set PL_LINKER_FLAGS='    
                                                    for flag in settings._linker_flags:
                                                        buffer += flag + " "
                                                else:
                                                    buffer += '@set PL_LINKER_FLAGS=\n'    
                                                    for flag in settings._linker_flags:
                                                        buffer += '@set PL_LINKER_FLAGS=' + flag + " %PL_LINKER_FLAGS%\n"
                                                buffer += "\n\n"

                                            if settings._link_libraries:
                                                buffer += '@rem libraries to link to\n'
                                                if project._collapse:
                                                    buffer += '@set PL_LINK_LIBRARIES='    
                                                    for link in settings._link_libraries:
                                                        buffer += link + " "
                                                else:
                                                    buffer += '@set PL_LINK_LIBRARIES=\n'    
                                                    for link in settings._link_libraries:
                                                        buffer += '@set PL_LINK_LIBRARIES=' + link + " %PL_LINK_LIBRARIES%\n"
                                                buffer += "\n"

                                            if target._target_type == TargetType.STATIC_LIBRARY:
                
                                                buffer += '@rem run compiler only\n'
                                                buffer += "@echo.\n"
                                                buffer += '@echo [1m[93mStep: ' + target._name +'[0m\n'
                                                buffer += '@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m\n'
                                                buffer += '@echo [1m[36mCompiling...[0m\n'
                                            
                                                buffer += '\n@rem each file must be compiled separately\n'
                                                for source in settings._source_files:
                                                    sub_buffer = ""
                                                    if settings._include_directories:
                                                        sub_buffer += " %PL_INCLUDE_DIRECTORIES%"
                                                    if settings._definitions:
                                                        sub_buffer += " %PL_DEFINES%"
                                                    if settings._compiler_flags:
                                                        sub_buffer += " %PL_COMPILER_FLAGS%"
                                                    buffer += 'cl -c' + sub_buffer + " " + source + ' -Fe"' + settings._output_directory + '/' + source + '.obj" -Fo"' + settings._output_directory + '/"'
                                                    buffer += "\n"

                                                buffer += "\n"
                                                buffer += "@rem check build status\n"
                                                buffer += "@set PL_BUILD_STATUS=%ERRORLEVEL%\n"

                                                buffer += "\n@rem if failed, skip linking\n"
                                                buffer += "@if %PL_BUILD_STATUS% NEQ 0 (\n"
                                                buffer += "    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%\n"
                                                buffer += "    @set PL_RESULT=[1m[91mFailed.[0m\n"
                                                buffer += "    goto " + 'Cleanup' + target._name
                                                buffer += "\n)\n"

                                                buffer += '\n@rem link object files into a shared lib\n'
                                                buffer += "@echo [1m[36mLinking...[0m\n"
                                                buffer += 'lib -nologo -OUT:"' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension + '" "' + settings._output_directory + '/*.obj"\n'

                                            elif target._target_type == TargetType.DYNAMIC_LIBRARY:

                                                buffer += "\n@rem source files\n"
                                                if project._collapse:
                                                    buffer += '@set PL_SOURCES='    
                                                    for source in settings._source_files:
                                                        buffer += '"' + source + '" '
                                                else:
                                                    buffer += '@set PL_SOURCES=\n'    
                                                    for source in settings._source_files:
                                                        buffer += '@set PL_SOURCES="' + source + '" %PL_SOURCES%\n'
                                                buffer += "\n\n"

                                                sub_buffer0 = ""
                                                sub_buffer1 = ""
                                                sub_buffer2 = ""
                                                if settings._include_directories:
                                                    sub_buffer0 += " %PL_INCLUDE_DIRECTORIES%"
                                                if settings._definitions:
                                                    sub_buffer0 += " %PL_DEFINES%"
                                                if settings._compiler_flags:
                                                    sub_buffer0 += " %PL_COMPILER_FLAGS%"
                                                if settings._linker_flags:
                                                    sub_buffer1 = " %PL_LINKER_FLAGS%"
                                                if settings._link_directories:
                                                    sub_buffer2 += " %PL_LINK_DIRECTORIES%"
                                                if settings._link_libraries:
                                                    sub_buffer2 += " %PL_LINK_LIBRARIES%"

                                                buffer += '@rem run compiler (and linker)\n'
                                                buffer += "@echo.\n"
                                                buffer += '@echo [1m[93mStep: ' + target._name +'[0m\n'
                                                buffer += '@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m\n'
                                                buffer += '@echo [1m[36mCompiling and Linking...[0m\n'
                                                buffer += 'cl' + sub_buffer0 + ' %PL_SOURCES% -Fe"' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension + '" -Fo"' + settings._output_directory + '/" -LD -link' + sub_buffer1 + ' -PDB:"' + settings._output_directory + '/' + settings._output_binary + '_%random%.pdb"' + sub_buffer2 + "\n\n"

                                                buffer += "@rem check build status\n"
                                                buffer += "@set PL_BUILD_STATUS=%ERRORLEVEL%\n"

                                                buffer += "\n@rem failed\n"
                                                buffer += "@if %PL_BUILD_STATUS% NEQ 0 (\n"
                                                buffer += "    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%\n"
                                                buffer += "    @set PL_RESULT=[1m[91mFailed.[0m\n"
                                                buffer += "    goto " + 'Cleanup' + target._name
                                                buffer += "\n)\n"

                                            elif target._target_type == TargetType.EXECUTABLE:

                                                
                                                buffer += "\n@rem source files\n"
                                                if project._collapse:
                                                    buffer += '@set PL_SOURCES='    
                                                    for source in settings._source_files:
                                                        buffer += '"' + source + '" '
                                                else:
                                                    buffer += '@set PL_SOURCES=\n'    
                                                    for source in settings._source_files:
                                                        buffer += '@set PL_SOURCES="' + source + '" %PL_SOURCES%\n'
                                                buffer += "\n\n"

                                                sub_buffer0 = ""
                                                sub_buffer1 = ""
                                                sub_buffer2 = ""
                                                if settings._include_directories:
                                                    sub_buffer0 += " %PL_INCLUDE_DIRECTORIES%"
                                                if settings._definitions:
                                                    sub_buffer0 += " %PL_DEFINES%"
                                                if settings._compiler_flags:
                                                    sub_buffer0 += " %PL_COMPILER_FLAGS%"
                                                if settings._linker_flags:
                                                    sub_buffer1 = " %PL_LINKER_FLAGS%"
                                                if settings._link_directories:
                                                    sub_buffer2 += " %PL_LINK_DIRECTORIES%"
                                                if settings._link_libraries:
                                                    sub_buffer2 += " %PL_LINK_LIBRARIES%"

                                                buffer += '@rem run compiler (and linker)\n'
                                                buffer += "@echo.\n"
                                                buffer += '@echo [1m[93mStep: ' + target._name +'[0m\n'
                                                buffer += '@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m\n'
                                                buffer += '@echo [1m[36mCompiling and Linking...[0m\n'

                                                buffer += '\n@rem skip actual compilation if hot reloading\n'
                                                buffer += '@if %PL_HOT_RELOAD_STATUS% equ 1 ( goto ' + 'Cleanup' + target._name + ' )\n'   
                                                
                                                buffer += '\n@rem call compiler\n'
                                                buffer += 'cl' + sub_buffer0 + ' %PL_SOURCES% -Fe"' + settings._output_directory + '/' + settings._output_binary + settings._output_binary_extension + '" -Fo"' + settings._output_directory + '/" -link' + sub_buffer1 + ' -PDB:"' + settings._output_directory + '/' + settings._output_binary + '_%random%.pdb"' + sub_buffer2 + "\n\n"
        
                                                buffer += "@rem check build status\n"
                                                buffer += "@set PL_BUILD_STATUS=%ERRORLEVEL%\n"
                                                buffer += "\n@rem failed\n"
                                                buffer += "@if %PL_BUILD_STATUS% NEQ 0 (\n"
                                                buffer += "    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%\n"
                                                buffer += "    @set PL_RESULT=[1m[91mFailed.[0m\n"
                                                buffer += "    goto " + 'Cleanup' + target._name
                                                buffer += "\n)\n"

                                            buffer += '\n@rem cleanup obj files\n'
                                            buffer += ':Cleanup' + target._name
                                            buffer += '\n    @echo [1m[36mCleaning...[0m\n'
                                            buffer += '    @del "' + settings._output_directory + '/*.obj"  > nul 2> nul'

                                            buffer += "\n\n"
                                            buffer += '@rem delete lock file\n'
                                            buffer += '@del "' + settings._output_directory + '/' + target._lock_file + '"'
                                            buffer += "\n\n"

                                            buffer += '@rem print results\n'
                                            buffer += "@echo.\n"
                                            buffer += "@echo [36mResult: [0m %PL_RESULT%\n"
                                            buffer += "@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m\n"
                                            buffer += "\n"
                if target_found:
                    buffer += '@rem ' + '~' * 40
                    buffer += '\n@rem end of ' + register_config + ' configuration\n'
                    buffer += "goto ExitLabel\n\n"

            buffer += ':ExitLabel\n'
            buffer += '@rem return CWD to previous CWD\n'
            buffer += '@popd'
            file.write(buffer)

