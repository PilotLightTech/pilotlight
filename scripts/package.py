import os
import sys
import platform
import shutil
import glob

debug_package = False

if len(sys.argv) > 1:
    for i in range(1, len(sys.argv)):
        if sys.argv[i] == "debug":
            debug_package = True

if not os.path.isdir("../out"):
    print("ERROR: Pilot Light not built", file=sys.stderr)
    quit()

if not os.path.isdir("../out/pilotlight"):
    os.mkdir("../out/pilotlight")

target_directory = "../out/pilotlight"

# scripts
scripts = [
    "pl_script_camera"
]

# extension headers
extension_headers = [
    "pl_platform_ext.h",
    "pl_console_ext.h",
    "pl_screen_log_ext.h",
    "pl_tools_ext.h",
    "pl_draw_backend_ext.h",
    "pl_draw_ext.h",
    "pl_ecs_ext.h",
    "pl_ecs_ext.inl",
    "pl_ecs_tools_ext.h",
    "pl_camera_ext.h",
    "pl_animation_ext.h",
    "pl_gizmo_ext.h",
    "pl_gpu_allocators_ext.h",
    "pl_graphics_ext.h",
    "pl_image_ext.h",
    "pl_job_ext.h",
    "pl_log_ext.h",
    "pl_model_loader_ext.h",
    "pl_profile_ext.h",
    "pl_rect_pack_ext.h",
    "pl_renderer_ext.h",
    "pl_resource_ext.h",
    "pl_shader_ext.h",
    "pl_stats_ext.h",
    "pl_string_intern_ext.h",
    "pl_ui_ext.h",
    "pl_physics_ext.h",
    "pl_collision_ext.h",
    "pl_bvh_ext.h",
    "pl_config_ext.h",
    "pl_starter_ext.h",
    "pl_mesh_ext.h",
    "pl_shader_variant_ext.h",
    "pl_vfs_ext.h",
    "pl_pak_ext.h",
    "pl_compress_ext.h",
    "pl_dds_ext.h",
    "pl_dxt_ext.h",
    "pl_datetime_ext.h",
    "pl_material_ext.h",
]

# extension binaries
extensions = [
    "pl_platform_ext",
    "pl_console_ext",
    "pl_screen_log_ext",
    "pl_tools_ext",
    "pl_draw_backend_ext",
    "pl_draw_ext",
    "pl_ecs_ext",
    "pl_animation_ext",
    "pl_camera_ext",
    "pl_ecs_tools_ext",
    "pl_gizmo_ext",
    "pl_gpu_allocators_ext",
    "pl_graphics_ext",
    "pl_image_ext",
    "pl_job_ext",
    "pl_log_ext",
    "pl_model_loader_ext",
    "pl_profile_ext",
    "pl_rect_pack_ext",
    "pl_renderer_ext",
    "pl_resource_ext",
    "pl_shader_ext",
    "pl_stats_ext",
    "pl_string_intern_ext",
    "pl_physics_ext",
    "pl_collision_ext",
    "pl_bvh_ext",
    "pl_config_ext",
    "pl_starter_ext",
    "pl_mesh_ext",
    "pl_shader_variant_ext",
    "pl_vfs_ext",
    "pl_pak_ext",
    "pl_datetime_ext",
    "pl_compress_ext",
    "pl_dds_ext",
    "pl_dxt_ext",
    "pl_material_ext",
    "pl_script_ext",
    "pl_ui_ext"
]

if debug_package:
    for i in range(len(extensions)):
        extensions[i] = extensions[i] + "d"

extensions.append("pl_unity_ext")

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

#-----------------------------------------------------------------------------
# [SECTION] files
#-----------------------------------------------------------------------------

# copy core headers
shutil.copy("../src/pl.h", target_directory + "/include/pl.h")
shutil.copy("../src/pl_config.h", target_directory + "/include/pl_config.h")

# copy simple extension headers
for extension in extension_headers:
    shutil.copy("../extensions/" + extension, target_directory + "/include/" + extension)

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
shutil.copy("../libs/pl_test.h", target_directory + "/include/pl_test.h")

# copy stb libs
shutil.copy("../dependencies/stb/stb_sprintf.h", target_directory + "/include/stb_sprintf.h")

# copy extension binary
for extension in extensions:

    if platform.system() == "Windows":
        shutil.move("../out/" + extension + ".dll", target_directory + "/bin/")
        if debug_package:
            for file in glob.glob("../out/" + extension + "_*.pdb"):
                shutil.move(file, target_directory + "/bin/")
    elif platform.system() == "Darwin":
        shutil.move("../out/" + extension + ".dylib", target_directory + "/bin/")
        if debug_package:
            shutil.copytree("../out/" + extension + ".dylib.dSYM", target_directory + "/bin/" + extension + ".dylib.dSYM")
    elif platform.system() == "Linux":
        shutil.move("../out/" + extension + ".so", target_directory + "/bin/")

# copy scripts
for script in scripts:
    if platform.system() == "Windows":
        shutil.move("../out/" + script + ".dll", target_directory + "/bin/")
        for file in glob.glob("../out/" + script + "d_*.pdb"):
            shutil.move(file, target_directory + "/bin/")
    elif platform.system() == "Darwin":
        shutil.move("../out/" + script + ".dylib", target_directory + "/bin/")
        if debug_package:
            shutil.copytree("../out/" + script + "d.dylib.dSYM", target_directory + "/bin/" + script + "d.dylib.dSYM")
            
    elif platform.system() == "Linux":
        shutil.move("../out/" + script + ".so", target_directory + "/bin/")

# copy libs & executable
if platform.system() == "Windows":
    
    if debug_package:
        shutil.move("../out/pilot_lightd.exe", target_directory + "/bin/")
        for file in glob.glob("../out/pilot_lightd_*.pdb"):
            shutil.move(file, target_directory + "/bin/")
        shutil.move("../src/vc140.pdb", target_directory + "/bin/")
    else:
        shutil.move("../out/pilot_light.exe", target_directory + "/bin/")
elif platform.system() == "Darwin":
    shutil.move("../out/pilot_light", target_directory + "/bin/")
    if debug_package:
        shutil.copytree("../out/pilot_light.dSYM", target_directory + "/bin/pilot_light.dSYM")
elif platform.system() == "Linux":
    shutil.move("../out/pilot_light", target_directory + "/bin/")

# copy shaders
for file in glob.glob("../out/*.spv"):
    shutil.move(file, target_directory + "/bin/")

#-----------------------------------------------------------------------------
# [SECTION] zip
#-----------------------------------------------------------------------------

if platform.system() == "Windows":
    if debug_package:
        shutil.make_archive("../out/pilotlight_win32d", "zip", "../out/pilotlight")
    else:
        shutil.make_archive("../out/pilotlight_win32", "zip", "../out/pilotlight")
elif platform.system() == "Darwin" and os.uname().machine == "arm64":
    if debug_package:
        shutil.make_archive("../out/pilotlight_macos_arm64d", "zip", "../out/pilotlight")
    else:
        shutil.make_archive("../out/pilotlight_macos_arm64", "zip", "../out/pilotlight")
elif platform.system() == "Darwin":
    if debug_package:
        shutil.make_archive("../out/pilotlight_macosd", "zip", "../out/pilotlight")
    else:
        shutil.make_archive("../out/pilotlight_macos", "zip", "../out/pilotlight")
elif platform.system() == "Linux":
    if debug_package:
        shutil.make_archive("../out/pilotlight_linux_amd64d", "zip", "../out/pilotlight")
    else:
        shutil.make_archive("../out/pilotlight_linux_amd64", "zip", "../out/pilotlight")

shutil.rmtree("../out/pilotlight")