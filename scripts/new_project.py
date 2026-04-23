import os
import sys
import shutil
import glob
import subprocess

target_directory = "../../project"

cpp = False
api = False
newProject = True

if len(sys.argv) > 1:
    target_directory = sys.argv[1]
    for i in range(2, len(sys.argv)):
        if sys.argv[i] == "cpp":
            cpp = True
        elif sys.argv[i] == "api":
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


for file in glob.glob("../extensions/pl_*_ext.h"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/include/")

shutil.copy("../src/pl.h", target_directory + "/dependencies/pilotlight/include/pl.h")
shutil.copy("../src/pl.inc", target_directory + "/dependencies/pilotlight/include/pl.inc")
shutil.copy("../src/pl_config.h", target_directory + "/dependencies/pilotlight/include/pl_config.h")
shutil.copy("../src/pl_internal.h", target_directory + "/dependencies/pilotlight/src/pl_internal.h")
shutil.copy("../src/pl_main_macos.m", target_directory + "/dependencies/pilotlight/src/pl_main_macos.m")

for file in glob.glob("../src/*.c*"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/src/")

for file in glob.glob("../libs/*"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/include/")

for file in glob.glob("../extensions/pl_*_ext.inl"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/include/")

for file in glob.glob("../extensions/pl_*_ext.inc"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/include/")

for file in glob.glob("../extensions/*.c*"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/src/")

for file in glob.glob("../extensions/*.m"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/src/")

for file in glob.glob("../extensions/*internal.h"):
    shutil.copy(file, target_directory + "/dependencies/pilotlight/src/")

shutil.copytree("../dependencies/cgltf", target_directory + "/dependencies/cgltf")
shutil.copytree("../dependencies/glfw", target_directory + "/dependencies/glfw")
shutil.copytree("../dependencies/imgui", target_directory + "/dependencies/imgui")
shutil.copytree("../dependencies/stb", target_directory + "/dependencies/stb")
shutil.copytree("../shaders", target_directory + "/dependencies/pilotlight/shaders")

if newProject:
    print("Generating New Project")
    if cpp and api:
        shutil.copy("../internal/templates/gen_build_cpp_api.py", target_directory + "/scripts/gen_build.py")
        shutil.copy("../internal/templates/template_app_api.cpp", target_directory + "/src/app.cpp")
    elif api:
        shutil.copy("../internal/templates/gen_build_c_api.py", target_directory + "/scripts/gen_build.py")
        shutil.copy("../internal/templates/template_app_api.c", target_directory + "/src/app.c")
    elif cpp:
        shutil.copy("../internal/templates/gen_build_c.py", target_directory + "/scripts/gen_build.py")
        shutil.copy("../internal/templates/template_app.c", target_directory + "/src/app.c")
    else:
        shutil.copy("../internal/templates/gen_build_c.py", target_directory + "/scripts/gen_build.py")
        shutil.copy("../internal/templates/template_app.c", target_directory + "/src/app.c")

    shutil.copy("../internal/templates/template_README.md", target_directory + "/README.md")
    shutil.copy("../internal/templates/template_gitignore", target_directory + "/.gitignore")
    shutil.copy("../internal/templates/template_c_cpp_properties.json", target_directory + "/.vscode/c_cpp_properties.json")
    shutil.copy("../internal/templates/template_launch.json", target_directory + "/.vscode/launch.json")

    os.chdir(target_directory)
    os.chdir("scripts")
    subprocess.run([os.path.basename(sys.executable), "gen_build.py"])

