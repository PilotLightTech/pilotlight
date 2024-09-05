__version__ = "1.0.0"

###############################################################################
#                                  Info                                       #
###############################################################################

# very poorly written & dirty system, to be cleaned up later

###############################################################################
#                                 Modules                                     #
###############################################################################

from enum import Enum
from contextlib import contextmanager

###############################################################################
#                                  Enums                                      #
###############################################################################

class TargetType(Enum):
    NONE = 0
    STATIC_LIBRARY = 0
    DYNAMIC_LIBRARY = 1
    EXECUTABLE = 2

###############################################################################
#                                 Classes                                     #
###############################################################################


class CompilerSettings:

    def __init__(self, name: str):

        self.name = name
        self.output_directory = None
        self.output_binary = None
        self.output_binary_extension = None
        self.definitions = []
        self.compiler_flags = []
        self.linker_flags = []
        self.include_directories = []
        self.link_directories = []
        self.source_files = []
        self.static_link_libraries = []
        self.dynamic_link_libraries = []
        self.link_frameworks = []
        self.target_type = TargetType.NONE
        self.pre_build_step = None
        self.post_build_step = None

        # inherited from platform
        self.platform_name = None

        # inherited from config
        self.config_name = None

        # inherited from target
        self.target_name = None
        self.target_type = None
        self.lock_file = None
        self.reloadable = None

        # inherited from project
        self.project_name = None
        self.reload_target_name = None
        self.working_directory = None
        self.registered_configurations = []

class CompilerProfile:

    def __init__(self, compilers = None, platforms = None, configurations = None, targets = None):

        self.compilers = compilers
        self.platforms = platforms
        self.configurations = configurations
        self.targets = targets

        self.output_directory = None
        self.definitions = []
        self.include_directories = []
        self.link_directories = []
        self.static_link_libraries = []
        self.dynamic_link_libraries = []
        self.link_frameworks = []
        self.source_files = []
        self.compiler_flags = []
        self.linker_flags = []
        self.output_binary_extension = None

    def is_active(self):

        if self.targets is not None:

            found = False
            for target in self.targets:
                if target == _context.target_name:
                    found = True
                    break
            if not found:
                return False
            
        if self.configurations is not None:

            found = False
            for config_name in self.configurations:
                if config_name == _context.config_name:
                    found = True
                    break
            if not found:
                return False
            
        if self.platforms is not None:

            found = False
            for platform_name in self.platforms:
                if platform_name == _context.platform_name:
                    found = True
                    break
            if not found:
                return False
            
        if self.compilers is not None:

            found = False
            for name in self.compilers:
                if name == _context.working_settings.name:
                    found = True
                    break
            if not found:
                return False
        
        return True

        
class BuildContext:
    
    def __init__(self):
        
        # persistent data
        self.current_settings = []

        # current project
        self.project_name = None
        self.reload_target_name = None
        self.working_directory = None
        self.registered_configurations = []
        self.profiles = []

        # current target
        self.target_name = None
        self.target_type = None
        self.target_lock_file = None
        self.target_reloadable = False

        # current config
        self.config_name = None

        # current platform
        self.platform_name = None

        # working settings
        self.working_settings = None

        # project scope
        self._project_output_directory = None
        self._project_definitions = []
        self._project_include_directories = []
        self._project_link_directories = []
        self._project_static_link_libraries = []
        self._project_dynamic_link_libraries = []
        self._project_link_frameworks = []
        self._project_source_files = []

        # target scope
        self._target_output_binary = None
        self._target_output_directory = None
        self._target_definitions = []
        self._target_include_directories = []
        self._target_link_directories = []
        self._target_static_link_libraries = []
        self._target_dynamic_link_libraries = []
        self._target_link_frameworks = []
        self._target_source_files = []

        # config scope
        self._config_output_binary = None
        self._config_output_directory = None
        self._config_definitions = []
        self._config_include_directories = []
        self._config_link_directories = []
        self._config_static_link_libraries = []
        self._config_dynamic_link_libraries = []
        self._config_link_frameworks = []
        self._config_source_files = []

        # platform scope
        self._platform_output_binary = None
        self._platform_output_directory = None
        self._platform_definitions = []
        self._platform_include_directories = []
        self._platform_link_directories = []
        self._platform_static_link_libraries = []
        self._platform_dynamic_link_libraries = []
        self._platform_link_frameworks = []
        self._platform_source_files = []

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
        # persistent data
        _context.current_settings = []

        # current project
        _context.project_name = name
        _context.reload_target_name = None
        _context.working_directory = "./"
        _context.registered_configurations = []
        _context.profiles = []

        # current target
        _context.target_name = None
        _context.target_type = None
        _context.target_lock_file = None
        _context.target_reloadable = False

        # current config
        _context.config_name = None

        # current platform
        _context.platform_name = None

        # working settings
        _context.working_settings = None

        # project scope
        _context._project_output_directory = None
        _context._project_definitions = []
        _context._project_include_directories = []
        _context._project_link_directories = []
        _context._project_static_link_libraries = []
        _context._project_dynamic_link_libraries = []
        _context._project_link_frameworks = []
        _context._project_source_files = []

        yield None
    finally:
        pass

def add_configuration(name: str):
    _context.registered_configurations.append(name)
    
def set_working_directory(directory: str):
    _context.working_directory = directory

def set_hot_reload_target(target_name: str):
    _context.reload_target_name = target_name

###############################################################################
#                                 Target                                      #
###############################################################################

@contextmanager
def target(name: str, target_type: TargetType, reloadable: bool = False):
    try:
        _context.target_name = name
        _context.target_type = target_type
        _context.target_lock_file = "lock.tmp"
        _context.target_reloadable = reloadable
        yield None
    finally:
        _context.target_name = None
        _context.target_type = None
        _context.target_lock_file = None
        _context.target_reloadable = False
        _context._target_output_directory = None
        _context._target_definitions = []
        _context._target_include_directories = []
        _context._target_link_directories = []
        _context._target_static_link_libraries = []
        _context._target_dynamic_link_libraries = []
        _context._target_link_frameworks = []
        _context._target_source_files = []

###############################################################################
#                             Configuration                                   #
###############################################################################

@contextmanager
def configuration(name: str):
    try:
        _context.config_name = name
        yield None
    finally:
        _context.config_name = None
        _context._config_output_directory = None
        _context._config_definitions = []
        _context._config_include_directories = []
        _context._config_link_directories = []
        _context._config_static_link_libraries = []
        _context._config_dynamic_link_libraries = []
        _context._config_link_frameworks = []
        _context._config_source_files = []

###############################################################################
#                                 Platform                                    #
###############################################################################

@contextmanager
def platform(name: str):
    try:
        _context.platform_name = name
        yield None
    finally:
        _context.platform_name = None
        _context._platform_output_directory = None
        _context._platform_definitions = []
        _context._platform_include_directories = []
        _context._platform_link_directories = []
        _context._platform_static_link_libraries = []
        _context._platform_dynamic_link_libraries = []
        _context._platform_link_frameworks = []
        _context._platform_source_files = []

###############################################################################
#                               Compiler                                      #
###############################################################################

@contextmanager
def compiler(name: str):
    try:
        compiler = CompilerSettings(name)

        # inherited from platform
        compiler.platform_name = _context.platform_name

        # inherited from config
        compiler.config_name = _context.config_name

        # inherited from target
        compiler.target_name = _context.target_name
        compiler.target_type = _context.target_type
        compiler.lock_file = _context.target_lock_file
        compiler.reloadable = _context.target_reloadable

        # inherited from project
        compiler.project_name = _context.project_name
        compiler.reload_target_name = _context.reload_target_name
        compiler.working_directory = _context.working_directory
        compiler.registered_configurations = _context.registered_configurations

        # inherited
        if _context._platform_output_directory is not None:
            compiler.output_directory = _context._platform_output_directory
        elif _context._config_output_directory is not None:
            compiler.output_directory = _context._config_output_directory
        elif _context._target_output_directory is not None:
            compiler.output_directory = _context._target_output_directory
        elif _context._project_output_directory is not None:
            compiler.output_directory = _context._project_output_directory

        if _context._platform_output_binary is not None:
            compiler.output_binary = _context._platform_output_binary
        elif _context._config_output_binary is not None:
            compiler.output_binary = _context._config_output_binary
        elif _context._target_output_binary is not None:
            compiler.output_binary = _context._target_output_binary

        compiler.link_directories = _context._project_link_directories + _context._target_link_directories + _context._config_link_directories + _context._platform_link_directories
        compiler.definitions = _context._project_definitions + _context._target_definitions + _context._config_definitions + _context._platform_definitions
        compiler.include_directories = _context._project_include_directories + _context._target_include_directories + _context._config_include_directories + _context._platform_include_directories
        compiler.static_link_libraries = _context._project_static_link_libraries + _context._target_static_link_libraries + _context._config_static_link_libraries + _context._platform_static_link_libraries
        compiler.dynamic_link_libraries = _context._project_dynamic_link_libraries + _context._target_dynamic_link_libraries + _context._config_dynamic_link_libraries + _context._platform_dynamic_link_libraries
        compiler.link_frameworks = _context._project_link_frameworks + _context._target_link_frameworks + _context._config_link_frameworks + _context._platform_link_frameworks
        compiler.source_files = _context._project_source_files + _context._target_source_files + _context._config_source_files + _context._platform_source_files

        _context.working_settings = compiler

        # check profiles
        for profile in _context.profiles:
            if profile.is_active():
                _context.working_settings.definitions.extend(profile.definitions)
                _context.working_settings.compiler_flags.extend(profile.compiler_flags)
                _context.working_settings.include_directories.extend(profile.include_directories)
                _context.working_settings.link_directories.extend(profile.link_directories)
                _context.working_settings.static_link_libraries.extend(profile.static_link_libraries)
                _context.working_settings.dynamic_link_libraries.extend(profile.dynamic_link_libraries)
                _context.working_settings.link_frameworks.extend(profile.link_frameworks)
                _context.working_settings.source_files.extend(profile.source_files)
                _context.working_settings.linker_flags.extend(profile.linker_flags)
                if _context.working_settings.output_directory is None:
                    _context.working_settings.output_directory = profile.output_directory
                if _context.working_settings.output_binary_extension is None:
                    _context.working_settings.output_binary_extension = profile.output_binary_extension

        yield _context.working_settings
    finally:
        _context.current_settings.append(_context.working_settings)
        _context.working_settings = None

def add_source_files(*args):

    if _context.working_settings is not None:
        for arg in args:
            _context.working_settings.source_files.append(arg)
    elif _context.platform_name is not None:
        for arg in args:
            _context._platform_source_files.append(arg)
    elif _context.config_name is not None:
        for arg in args:
            _context._config_source_files.append(arg)
    elif _context.target_name is not None:
        for arg in args:
            _context._target_source_files.append(arg)
    elif _context.project_name is not None:
        for arg in args:
            _context._project_source_files.append(arg)
    else:
        raise Exception("'add_source_files(...)' must be called within a scope")

def add_static_link_libraries(*args):
    if _context.working_settings is not None:
        for arg in args:
            _context.working_settings.static_link_libraries.append(arg)
    elif _context.platform_name is not None:
        for arg in args:
            _context._platform_static_link_libraries.append(arg)
    elif _context.config_name is not None:
        for arg in args:
            _context._config_static_link_libraries.append(arg)
    elif _context.target_name is not None:
        for arg in args:
            _context._target_static_link_libraries.append(arg)
    elif _context.project_name is not None:
        for arg in args:
            _context._project_static_link_libraries.append(arg)
    else:
        raise Exception("'add_static_link_libraries(...)' must be called within a scope")

def add_dynamic_link_libraries(*args):

    if _context.working_settings is not None:
        for arg in args:
            _context.working_settings.dynamic_link_libraries.append(arg)
    elif _context.platform_name is not None:
        for arg in args:
            _context._platform_dynamic_link_libraries.append(arg)
    elif _context.config_name is not None:
        for arg in args:
            _context._config_dynamic_link_libraries.append(arg)
    elif _context.target_name is not None:
        for arg in args:
            _context._target_dynamic_link_libraries.append(arg)
    elif _context.project_name is not None:
        for arg in args:
            _context._project_dynamic_link_libraries.append(arg)
    else:
        raise Exception("'add_dynamic_link_libraries(...)' must be called within a scope")

def add_link_frameworks(*args):

    if _context.working_settings is not None:
        for arg in args:
            _context.working_settings.link_frameworks.append(arg)
    elif _context.platform_name is not None:
        for arg in args:
            _context._platform_link_frameworks.append(arg)
    elif _context.config_name is not None:
        for arg in args:
            _context._config_link_frameworks.append(arg)
    elif _context.target_name is not None:
        for arg in args:
            _context._target_link_frameworks.append(arg)
    elif _context.project_name is not None:
        for arg in args:
            _context._project_link_frameworks.append(arg)
    else:
        raise Exception("'add_link_frameworks(...)' must be called within a scope")

def add_definitions(*args):

    if _context.working_settings is not None:
        for arg in args:
            _context.working_settings.definitions.append(arg)
    elif _context.platform_name is not None:
        for arg in args:
            _context._platform_definitions.append(arg)
    elif _context.config_name is not None:
        for arg in args:
            _context._config_definitions.append(arg)
    elif _context.target_name is not None:
        for arg in args:
            _context._target_definitions.append(arg)
    elif _context.project_name is not None:
        for arg in args:
            _context._project_definitions.append(arg)
    else:
        raise Exception("'add_definitions(...)' must be called within a scope")

def add_compiler_flags(*args):
    for arg in args:
        _context.working_settings.compiler_flags.append(arg)

def add_linker_flags(*args):
    for arg in args:
        _context.working_settings.linker_flags.append(arg)

def add_include_directories(*args):
    if _context.working_settings is not None:
        for arg in args:
            _context.working_settings.include_directories.append(arg)
    elif _context.platform_name is not None:
        for arg in args:
            _context._platform_include_directories.append(arg)
    elif _context.config_name is not None:
        for arg in args:
            _context._config_include_directories.append(arg)
    elif _context.target_name is not None:
        for arg in args:
            _context._target_include_directories.append(arg)
    elif _context.project_name is not None:
        for arg in args:
            _context._project_include_directories.append(arg)
    else:
        raise Exception("'add_include_directories(...)' must be called within a scope")

def add_link_directories(*args):

    if _context.working_settings is not None:
        for arg in args:
            _context.working_settings.link_directories.append(arg)
    elif _context.platform_name is not None:
        for arg in args:
            _context._platform_link_directories.append(arg)
    elif _context.config_name is not None:
        for arg in args:
            _context._config_link_directories.append(arg)
    elif _context.target_name is not None:
        for arg in args:
            _context._target_link_directories.append(arg)
    elif _context.project_name is not None:
        for arg in args:
            _context._project_link_directories.append(arg)
    else:
        raise Exception("'add_link_directories(...)' must be called within a scope")

def set_output_binary(binary: str):

    if _context.working_settings is not None:
        _context.working_settings.output_binary = binary
    elif _context.platform_name is not None:
        _context._platform_output_binary = binary
    elif _context.config_name is not None:
        _context._config_output_binary = binary
    elif _context.target_name is not None:
        _context._target_output_binary = binary
    else:
        raise Exception("'set_output_binary(...)' must be called within a correct scope")

def set_output_binary_extension(extension: str):
    _context.working_settings.output_binary_extension = extension

def set_output_directory(directory: str):

    if _context.working_settings is not None:
        _context.working_settings.output_directory = directory
    elif _context.platform_name is not None:
        _context._platform_output_directory = directory
    elif _context.config_name is not None:
        _context._config_output_directory = directory
    elif _context.target_name is not None:
        _context._target_output_directory = directory
    elif _context.project_name is not None:
        _context._project_output_directory = directory
    else:
        raise Exception("'set_output_directory(...)' must be called within a scope")

def add_compiler_flags_profile(targets, configurations, platforms, compilers, *args):
    profile = CompilerProfile(compilers, platforms, configurations, targets)
    profile.compiler_flags = args
    _context.profiles.append(profile)

def add_include_directories_profile(targets, configurations, platforms, compilers, *args):
    profile = CompilerProfile(compilers, platforms, configurations, targets)
    profile.include_directories = args
    _context.profiles.append(profile)

def add_definitions_profile(targets, configurations, platforms, compilers, *args):
    profile = CompilerProfile(compilers, platforms, configurations, targets)
    profile.definitions = args
    _context.profiles.append(profile)

def add_link_directories_profile(targets, configurations, platforms, compilers, *args):
    profile = CompilerProfile(compilers, platforms, configurations, targets)
    profile.link_directories = args
    _context.profiles.append(profile)

def add_static_link_libraries_profile(targets, configurations, platforms, compilers, *args):
    profile = CompilerProfile(compilers, platforms, configurations, targets)
    profile.static_link_libraries = args
    _context.profiles.append(profile)

def add_dynamic_link_libraries_profile(targets, configurations, platforms, compilers, *args):
    profile = CompilerProfile(compilers, platforms, configurations, targets)
    profile.dynamic_link_libraries = args
    _context.profiles.append(profile)

def add_link_frameworks_profile(targets, configurations, platforms, compilers, *args):
    profile = CompilerProfile(compilers, platforms, configurations, targets)
    profile.link_frameworks = args
    _context.profiles.append(profile)

def add_source_files_profile(targets, configurations, platforms, compilers, *args):
    profile = CompilerProfile(compilers, platforms, configurations, targets)
    profile.source_files = args
    _context.profiles.append(profile)

def add_linker_flags_profile(targets, configurations, platforms, compilers, *args):
    profile = CompilerProfile(compilers, platforms, configurations, targets)
    profile.linker_flags = args
    _context.profiles.append(profile)

def set_output_binary_extension_profile(targets, configurations, platforms, compilers, extension):
    profile = CompilerProfile(compilers, platforms, configurations, targets)
    profile.output_binary_extension = extension
    _context.profiles.append(profile)

def set_output_directory_profile(targets, configurations, platforms, compilers, irectory):
    profile = CompilerProfile(compilers, platforms, configurations, targets)
    profile.output_directory = irectory
    _context.profiles.append(profile)

def set_pre_target_build_step(code: str):
    _context.working_settings.pre_build_step = code

def set_post_target_build_step(code: str):
    _context.working_settings.post_build_step = code

###############################################################################
#                                Query                                        #
###############################################################################

def get_platform() -> str:
    return _context.platform_name

def get_target() -> str:
    return _context.target_name

def get_target_type() -> str:
    return _context.target_type

def get_configuration() -> str:
    return _context.config_name

def get_compiler() -> str:
    return _context.working_settings.name
