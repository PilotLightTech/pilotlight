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

extensions = [
    "pl_debug_ext",
    "pl_graphics_ext",
    "pl_image_ext",
    "pl_stats_ext"
]

if os.path.isdir(target_directory):
    shutil.rmtree(target_directory)

os.mkdir(target_directory)

###############################################################################
#                                 structure                                   #
###############################################################################

if not os.path.isdir(target_directory + "/include"):
    os.mkdir(target_directory + "/include")

if not os.path.isdir(target_directory + "/bin"):
    os.mkdir(target_directory + "/bin")

if not os.path.isdir(target_directory + "/lib"):
    os.mkdir(target_directory + "/lib")

###############################################################################
#                                   files                                     #
###############################################################################

# copy core headers
shutil.copy("../src/pilotlight.h", target_directory + "/include/pilotlight.h")
shutil.copy("../src/pl_config.h", target_directory + "/include/pl_config.h")
shutil.copy("../src/pl_os.h", target_directory + "/include/pl_os.h")

# copy extension headers
for extension in extensions:
    shutil.copy("../extensions/" + extension + ".h", target_directory + "/include/" + extension + ".h")

# copy pilotlight-lib headers
shutil.copy("../dependencies/pilotlight-libs/pl_ds.h", target_directory + "/include/pl_ds.h")
shutil.copy("../dependencies/pilotlight-libs/pl_log.h", target_directory + "/include/pl_log.h")
shutil.copy("../dependencies/pilotlight-libs/pl_profile.h", target_directory + "/include/pl_profile.h")
shutil.copy("../dependencies/pilotlight-libs/pl_memory.h", target_directory + "/include/pl_memory.h")
shutil.copy("../dependencies/pilotlight-libs/pl_math.h", target_directory + "/include/pl_math.h")
shutil.copy("../dependencies/pilotlight-libs/pl_json.h", target_directory + "/include/pl_json.h")
shutil.copy("../dependencies/pilotlight-libs/pl_stl.h", target_directory + "/include/pl_stl.h")
shutil.copy("../dependencies/pilotlight-libs/pl_string.h", target_directory + "/include/pl_string.h")

# copy pilotlight-ui headers
shutil.copy("../dependencies/pilotlight-ui/pl_ui.h", target_directory + "/include/pl_ui.h")

# copy stb libs
shutil.copy("../dependencies/stb/stb_sprintf.h", target_directory + "/include/stb_sprintf.h")

# copy extensions
for extension in extensions:
    if platform.system() == "Windows":
        shutil.copy("../out/" + extension + ".dll", target_directory + "/bin/" + extension + ".dll")
    elif platform.system() == "Darwin":
        shutil.copy("../out/" + extension + ".dylib", target_directory + "/bin/" + extension + ".dylib")
        shutil.copytree("../out/" + extension + ".dylib.dSYM", target_directory + "/bin/" + extension + ".dylib.dSYM")
    elif platform.system() == "Linux":
        shutil.copy("../out/" + extension + ".so", target_directory + "/bin/" + extension + ".so")

# copy libs & executable
if platform.system() == "Windows":
    shutil.copy("../out/pilot_light.exe", target_directory + "/bin/pilot_light.exe")
    shutil.copy("../out/pilotlight.lib", target_directory + "/lib/pilotlight.lib")
    shutil.copy("../src/vc140.pdb", target_directory + "/bin/vc140.pdb")
elif platform.system() == "Darwin":
    shutil.copy("../out/pilot_light", target_directory + "/bin/pilot_light")
    shutil.copy("../out/pilotlight.a", target_directory + "/lib/pilotlight.a")
    shutil.copytree("../out/pilot_light.dSYM", target_directory + "/bin/pilot_light.dSYM")
elif platform.system() == "Linux":
    shutil.copy("../out/pilot_light", target_directory + "/bin/pilot_light")
    shutil.copy("../out/pilotlight.a", target_directory + "/lib/pilotlight.a")

shutil.copy("../apps/pl_config.json", target_directory + "/bin/pl_config.json")

###############################################################################
#                                     zip                                     #
###############################################################################

for extension in extensions:
    if platform.system() == "Windows":
        shutil.make_archive("../out/pilotlight_win32", "zip", "../out/pilotlight")
    elif platform.system() == "Darwin" and os.uname().machine == "arm64":
        shutil.make_archive("../out/pilotlight_macos_arm64", "zip", "../out/pilotlight")
    elif platform.system() == "Darwin":
        shutil.make_archive("../out/pilotlight_macos", "zip", "../out/pilotlight")
    elif platform.system() == "Linux":
        shutil.make_archive("../out/pilotlight_linux_amd64", "zip", "../out/pilotlight")

shutil.rmtree("../out/pilotlight")