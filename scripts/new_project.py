import os
import sys
import shutil
import glob
import subprocess

if len(sys.argv) <= 1:
    print("Pilot Light - New Project Script");
    print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    print("Usage: python new_project.py <name>");
    exit()

target_directory = "../../project"
file_directory = os.path.dirname(os.path.abspath(__file__))
build_sys_directory = os.path.dirname(os.path.abspath(__file__)) + "/.."

target_directory = sys.argv[1]

if os.path.isdir(target_directory + "/dependencies"):
    print("Project already exists");

os.mkdir(target_directory)

# default assets
shutil.copytree(file_directory + "/../assets", target_directory + "/assets")

# basic directories
os.mkdir(target_directory + "/src")
os.mkdir(target_directory + "/scripts")
os.mkdir(target_directory + "/shaders")
os.mkdir(target_directory + "/resources")
os.mkdir(target_directory + "/docs")
os.mkdir(target_directory + "/tests")
os.mkdir(target_directory + "/build")
os.mkdir(target_directory + "/.vscode")

# dependencies
os.mkdir(target_directory + "/dependencies")
os.mkdir(target_directory + "/dependencies/pilotlight")
os.mkdir(target_directory + "/dependencies/pilotlight/include")
os.mkdir(target_directory + "/dependencies/pilotlight/src")

# shaders
shutil.copytree(file_directory + "/../shaders", target_directory + "/dependencies/pilotlight/shaders")

# headers
shutil.copy(file_directory + "/../src/pl.h", target_directory + "/dependencies/pilotlight/include/pl.h")
shutil.copy(file_directory + "/../src/pl.inc", target_directory + "/dependencies/pilotlight/include/pl.inc")
shutil.copy(file_directory + "/../src/pl_internal.h", target_directory + "/dependencies/pilotlight/src/pl_internal.h")
for file in glob.glob(file_directory + "/../extensions/pl_*_ext.h"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/include/")

# implementation files
shutil.copy(file_directory + "/../src/pl_main_macos.m", target_directory + "/dependencies/pilotlight/src/pl_main_macos.m")
for file in glob.glob(file_directory + "/../src/*.c*"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/src/")
for file in glob.glob(file_directory + "/../extensions/*.c*"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/src/")
for file in glob.glob(file_directory + "/../extensions/*.m"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/src/")

# header only libraries
for file in glob.glob(file_directory + "/../libs/*"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/include/")

# inline files
for file in glob.glob(file_directory + "/../extensions/pl_*_ext.inl"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/include/")

# include files
for file in glob.glob(file_directory + "/../extensions/pl_*_ext.inc"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/include/")

for file in glob.glob(file_directory + "/../extensions/*internal.h"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/src/")

# default resources
shutil.copy(file_directory + "/../resources/sky.hdr", target_directory + "/resources/sky.hdr")
shutil.copy(file_directory + "/../resources/fa-solid-900.otf", target_directory + "/resources/fa-solid-900.otf")
shutil.copy(file_directory + "/../resources/Cousine-Regular.ttf", target_directory + "/resources/Cousine-Regular.ttf")

# misc.
shutil.copy(file_directory + "/../src/pl_config.h", target_directory + "/src/pl_config.h")

# templates
shutil.copy(file_directory + "/../internal/template_gen_build_app.py", target_directory + "/scripts/gen_build_app.py")
shutil.copy(file_directory + "/../internal/template_app.cpp", target_directory + "/src/app.cpp")
shutil.copy(file_directory + "/../internal/template_gen_build.py", target_directory + "/scripts/gen_build.py")
shutil.copy(file_directory + "/../internal/template_gen_build_pilotlight.py", target_directory + "/scripts/gen_build_pilotlight.py")
shutil.copy(file_directory + "/../internal/template_README.md", target_directory + "/README.md")
shutil.copy(file_directory + "/../internal/template_gitignore", target_directory + "/.gitignore")
shutil.copy(file_directory + "/../internal/template_c_cpp_properties.json", target_directory + "/.vscode/c_cpp_properties.json")
shutil.copy(file_directory + "/../internal/template_launch.json", target_directory + "/.vscode/launch.json")

# build scripts for user
os.chdir(target_directory)
os.chdir("scripts")
subprocess.run([os.path.basename(sys.executable), "gen_build.py", build_sys_directory])