import os
import sys
import shutil
import glob
import subprocess

if len(sys.argv) <= 1:
    print("Pilot Light - New Project Script");
    print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    print("Usage: python new_project.py <name> [options]");
    print("\nOptions:");
    print("        -cpp    Generate C++ project");
    print("        -c      Generate C project (default)");
    print("        -hr     Generate project utilizing function pointers & hot reloading");
    exit()

target_directory = "../../project"
file_directory = os.path.dirname(os.path.abspath(__file__))

cpp = False
api = False
newProject = True

target_directory = sys.argv[1]
for i in range(2, len(sys.argv)):
    if sys.argv[i] == "-cpp":
        cpp = True
    elif sys.argv[i] == "-c":
        cpp = False
    elif sys.argv[i] == "-hr":
        api = True
    else:
        target_directory = sys.argv[i]

if os.path.isdir(target_directory + "/dependencies"):
    newProject = False
    print("Updating Existing Project")
    shutil.rmtree(target_directory + "/dependencies")

if not os.path.isdir(target_directory):
    os.mkdir(target_directory)
    os.mkdir(target_directory + "/src")
    os.mkdir(target_directory + "/scripts")
    os.mkdir(target_directory + "/shaders")
    os.mkdir(target_directory + "/docs")
    os.mkdir(target_directory + "/tests")
    os.mkdir(target_directory + "/.vscode")

os.mkdir(target_directory + "/dependencies")
os.mkdir(target_directory + "/dependencies/pilotlight")
os.mkdir(target_directory + "/dependencies/pilotlight/include")
os.mkdir(target_directory + "/dependencies/pilotlight/src")


for file in glob.glob(file_directory + "/../extensions/pl_*_ext.h"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/include/")

shutil.copy(file_directory + "/../src/pl.h", target_directory + "/dependencies/pilotlight/include/pl.h")
shutil.copy(file_directory + "/../src/pl.inc", target_directory + "/dependencies/pilotlight/include/pl.inc")
shutil.copy(file_directory + "/../src/pl_internal.h", target_directory + "/dependencies/pilotlight/src/pl_internal.h")
shutil.copy(file_directory + "/../src/pl_main_macos.m", target_directory + "/dependencies/pilotlight/src/pl_main_macos.m")

for file in glob.glob(file_directory + "/../src/*.c*"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/src/")

for file in glob.glob(file_directory + "/../libs/*"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/include/")

for file in glob.glob(file_directory + "/../extensions/pl_*_ext.inl"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/include/")

for file in glob.glob(file_directory + "/../extensions/pl_*_ext.inc"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/include/")

for file in glob.glob(file_directory + "/../extensions/*.c*"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/src/")

for file in glob.glob(file_directory + "/../extensions/*.m"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/src/")

for file in glob.glob(file_directory + "/../extensions/*internal.h"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/src/")

shutil.copytree(file_directory + "/../thirdparty/cgltf", target_directory + "/dependencies/cgltf")
shutil.copytree(file_directory + "/../thirdparty/glfw", target_directory + "/dependencies/glfw")
shutil.copytree(file_directory + "/../thirdparty/imgui", target_directory + "/dependencies/imgui")
shutil.copytree(file_directory + "/../thirdparty/stb", target_directory + "/dependencies/stb")
shutil.copytree(file_directory + "/../shaders", target_directory + "/dependencies/pilotlight/shaders")

if newProject:
    print("Generating New Project")
    if cpp and api:
        shutil.copy(file_directory + "/../internal/templates/gen_build_cpp_api.py", target_directory + "/scripts/gen_build.py")
        shutil.copy(file_directory + "/../internal/templates/template_app_api.cpp", target_directory + "/src/app.cpp")
    elif api:
        shutil.copy(file_directory + "/../internal/templates/gen_build_c_api.py", target_directory + "/scripts/gen_build.py")
        shutil.copy(file_directory + "/../internal/templates/template_app_api.cpp", target_directory + "/src/app.c")
    elif cpp:
        shutil.copy(file_directory + "/../internal/templates/gen_build_cpp.py", target_directory + "/scripts/gen_build.py")
        shutil.copy(file_directory + "/../internal/templates/template_app.cpp", target_directory + "/src/app.cpp")
    else:
        shutil.copy(file_directory + "/../internal/templates/gen_build_c.py", target_directory + "/scripts/gen_build.py")
        shutil.copy(file_directory + "/../internal/templates/template_app.cpp", target_directory + "/src/app.c")

    shutil.copy(file_directory + "/../src/pl_config.h", target_directory + "/src/pl_config.h")
    shutil.copy(file_directory + "/../internal/templates/template_README.md", target_directory + "/README.md")
    shutil.copy(file_directory + "/../internal/templates/template_gitignore", target_directory + "/.gitignore")
    shutil.copy(file_directory + "/../internal/templates/template_c_cpp_properties.json", target_directory + "/.vscode/c_cpp_properties.json")
    shutil.copy(file_directory + "/../internal/templates/template_launch.json", target_directory + "/.vscode/launch.json")

    os.chdir(target_directory)
    os.chdir("scripts")
    subprocess.run([os.path.basename(sys.executable), "gen_build.py"])

else:
    if(os.path.isdir(target_directory + "/out")):
        shutil.rmtree(target_directory + "/out")
    if(os.path.isdir(target_directory + "/shader-temp")):
        shutil.rmtree(target_directory + "/shader-temp")
    if(os.path.isdir(target_directory + "/cache")):
        shutil.rmtree(target_directory + "/cache")
    os.chdir(target_directory)
    os.chdir("scripts")
    subprocess.run([os.path.basename(sys.executable), "gen_build.py"])

