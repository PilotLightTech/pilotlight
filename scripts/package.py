import os
import sys
import platform
import shutil

if not os.path.isdir("../out"):
    print("ERROR: Pilot Light not built", file=sys.stderr)
    quit()

if not os.path.isdir("../out/pilotlight"):
    os.mkdir("../out/pilotlight")

target_directory = "../out/pilotlight"

# extensions
extension_headers = [
    "pl_debug_ext",
    "pl_draw_ext",
    "pl_ecs_ext",
    "pl_gpu_allocators_ext",
    "pl_graphics_ext",
    "pl_image_ext",
    "pl_job_ext",
    "pl_model_loader_ext",
    "pl_rect_pack_ext",
    "pl_ref_renderer_ext",
    "pl_resource_ext",
    "pl_shader_ext",
    "pl_stats_ext",
    "pl_ui_ext",
]

# scripts
scripts = [
    "pl_script_camera"
]

if os.path.isdir(target_directory):
    shutil.rmtree(target_directory)

os.mkdir(target_directory)

#-----------------------------------------------------------------------------
# [SECTION] structure
#-----------------------------------------------------------------------------

if not os.path.isdir(target_directory + "/include"):
    os.mkdir(target_directory + "/include")

if not os.path.isdir(target_directory + "/bin"):
    os.mkdir(target_directory + "/bin")

if not os.path.isdir(target_directory + "/lib"):
    os.mkdir(target_directory + "/lib")

#-----------------------------------------------------------------------------
# [SECTION] files
#-----------------------------------------------------------------------------

# copy core headers
shutil.copy("../src/pilot_light.h", target_directory + "/include/pilot_light.h")
shutil.copy("../src/pl_config.h", target_directory + "/include/pl_config.h")
shutil.copy("../src/pl_os.h", target_directory + "/include/pl_os.h")

# copy simple extension headers
for extension in extension_headers:
    shutil.copy("../extensions/" + extension + ".h", target_directory + "/include/" + extension + ".h")

# special headers
shutil.copy("../extensions/pl_script_ext.h", target_directory + "/include/pl_script_ext.h")

# copy pilotlight-lib headers
shutil.copy("../libs/pl_ds.h", target_directory + "/include/pl_ds.h")
shutil.copy("../libs/pl_log.h", target_directory + "/include/pl_log.h")
shutil.copy("../libs/pl_profile.h", target_directory + "/include/pl_profile.h")
shutil.copy("../libs/pl_memory.h", target_directory + "/include/pl_memory.h")
shutil.copy("../libs/pl_math.h", target_directory + "/include/pl_math.h")
shutil.copy("../libs/pl_json.h", target_directory + "/include/pl_json.h")
shutil.copy("../libs/pl_stl.h", target_directory + "/include/pl_stl.h")
shutil.copy("../libs/pl_string.h", target_directory + "/include/pl_string.h")

# copy stb libs
shutil.copy("../dependencies/stb/stb_sprintf.h", target_directory + "/include/stb_sprintf.h")

# copy extension binary
if platform.system() == "Windows":
    shutil.copy("../out/pilot_light.dll", target_directory + "/bin/pilot_light.dll")
elif platform.system() == "Darwin":
    shutil.copy("../out/pilot_light.dylib", target_directory + "/bin/pilot_light.dylib")
    shutil.copytree("../out/pilot_light.dylib.dSYM", target_directory + "/bin/pilot_light.dylib.dSYM")
elif platform.system() == "Linux":
    shutil.copy("../out/pilot_light.so", target_directory + "/bin/pilot_light.so")

# copy scripts
for script in scripts:
    if platform.system() == "Windows":
        shutil.copy("../out/" + script + ".dll", target_directory + "/bin/" + script + ".dll")
    elif platform.system() == "Darwin":
        shutil.copy("../out/" + script + ".dylib", target_directory + "/bin/" + script + ".dylib")
        shutil.copytree("../out/" + script + ".dylib.dSYM", target_directory + "/bin/" + script + ".dylib.dSYM")
    elif platform.system() == "Linux":
        shutil.copy("../out/" + script + ".so", target_directory + "/bin/" + script + ".so")

# copy libs & executable
if platform.system() == "Windows":
    shutil.copy("../out/pilot_light.exe", target_directory + "/bin/pilot_light.exe")
    shutil.copy("../out/pilot_light.lib", target_directory + "/lib/pilot_light.lib")
    shutil.copy("../src/vc140.pdb", target_directory + "/bin/vc140.pdb")
elif platform.system() == "Darwin":
    shutil.copy("../out/pilot_light", target_directory + "/bin/pilot_light")
    shutil.copy("../out/libpilot_light.a", target_directory + "/lib/libpilot_light.a")
    shutil.copytree("../out/pilot_light.dSYM", target_directory + "/bin/pilot_light.dSYM")
elif platform.system() == "Linux":
    shutil.copy("../out/pilot_light", target_directory + "/bin/pilot_light")
    shutil.copy("../out/pilot_light.a", target_directory + "/lib/pilot_light.a")

#-----------------------------------------------------------------------------
# [SECTION] zip
#-----------------------------------------------------------------------------

if platform.system() == "Windows":
    shutil.make_archive("../out/pilotlight_win32", "zip", "../out/pilotlight")
elif platform.system() == "Darwin" and os.uname().machine == "arm64":
    shutil.make_archive("../out/pilotlight_macos_arm64", "zip", "../out/pilotlight")
elif platform.system() == "Darwin":
    shutil.make_archive("../out/pilotlight_macos", "zip", "../out/pilotlight")
elif platform.system() == "Linux":
    shutil.make_archive("../out/pilotlight_linux_amd64", "zip", "../out/pilotlight")

shutil.rmtree("../out/pilotlight")