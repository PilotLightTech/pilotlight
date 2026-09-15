import os
import sys
import shutil
import glob
import subprocess

if len(sys.argv) <= 1:
    print("Pilot Light - Update Project Script");
    print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    print("Usage: python update_project.py <name>");
    exit()

target_directory = sys.argv[1]
file_directory = os.path.dirname(os.path.abspath(__file__))
build_sys_directory = os.path.dirname(os.path.abspath(__file__)) + "/.."

print("Updating Existing Project")

# remove older dependencies
if(os.path.isdir(target_directory + "/dependencies/pilotlight")):
    shutil.rmtree(target_directory + "/dependencies/pilotlight")

# clean caches
if(os.path.isdir(target_directory + "/out")):
    shutil.rmtree(target_directory + "/out")
if(os.path.isdir(target_directory + "/cache/shaders")):
    shutil.rmtree(target_directory + "/cache/shaders")

# dependencies
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

# implmentation files
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

shutil.copy(file_directory + "/../internal/template_gen_build_pilotlight.py", target_directory + "/scripts/gen_build_pilotlight.py")

# build scripts for user
os.chdir(target_directory)
os.chdir("scripts")
subprocess.run([os.path.basename(sys.executable), "gen_build.py", build_sys_directory])