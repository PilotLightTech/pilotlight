import os
import platform

defines = [
    "_DEBUG",
    "PL_PROFILE_ON",
    "PL_LOG_IMPLEMENTATION",
    "PL_MEMORY_IMPLEMENTATION",
    "PL_PROFILE_IMPLEMENTATION",
    "PL_LOG_ON",
    "PL_MEMORY_IMPLEMENTATION",
    "PL_STL_IMPLEMENTATION",
    "PL_STRING_IMPLEMENTATION",
    "PL_MATH_INCLUDE_FUNCTIONS",
    "PL_JSON_IMPLEMENTATION",
    "PL_VULKAN_BACKEND",
    "PL_METAL_BACKEND",
    "PL_TEST_IMPLEMENTATION",
    "PL_CONFIG_DEBUG",
    "PL_INCLUDE_SPIRV_CROSS",
    "PL_EXPERIMENTAL",
]

includes = [
    "${workspaceFolder}/**",
    "${workspaceFolder}/editor",
    "${workspaceFolder}/src",
    "${workspaceFolder}/libs",
    "${workspaceFolder}/extensions",
    "${workspaceFolder}/dependencies/stb",
    "${workspaceFolder}/dependencies/cgltf",
    "${workspaceFolder}/dependencies/imgui",
    "${workspaceFolder}/dependencies/glfw",
    "${env:VK_SDK_PATH}/Include"
]

########################################################################################################################
# setup vs code
########################################################################################################################

if not os.path.isdir('../.vscode'):
    os.mkdir('../.vscode')

with open('../.vscode/launch.json', 'w') as file:

    lines = []
    lines.append('{')
    lines.append('  "version": "0.2.0",')
    lines.append('  "configurations": [')
    lines.append('  {')

    if platform.system() == "Windows":
        lines.append('      "name": "(Windows) Launch",')
        lines.append('      "type": "cppvsdbg",')
        lines.append('      "program": "${workspaceFolder}/out/pilot_light.exe",')
        lines.append('      "console": "integratedTerminal",')

    elif platform.system() == "Darwin":
        lines.append('      "name": "(lldb) Launch",')
        lines.append('      "type": "cppdbg",')
        lines.append('      "targetArchitecture": "arm64",')
        lines.append('      "program": "${workspaceFolder}/out/pilot_light",')
        lines.append('      "externalConsole": false,')
        lines.append('      "MIMode": "lldb",')

    elif platform.system() == "Linux":
        lines.append('      "name": "(lldb) Launch",')
        lines.append('      "type": "cppdbg",')
        lines.append('      "program": "${workspaceFolder}/out/pilot_light",')

    lines.append('      "request": "launch",')
    lines.append('      "args": ["-a", "app", "-hr"],')
    lines.append('      "stopAtEntry": false,')
    lines.append('      "cwd": "${workspaceFolder}/out/",')
    lines.append('      "environment": []')
    lines.append('    }')
    lines.append('  ]')
    lines.append('}')

    for i in range(len(lines)):
        lines[i] = lines[i] + "\n"
    file.writelines(lines)

with open('../.vscode/c_cpp_properties.json', 'w') as file:
    lines = []
    lines.append('{')
    lines.append('  "version" : 4,')
    lines.append('  "configurations": [')
    lines.append('    {')

    if platform.system() == "Windows":
        lines.append('      "name": "Win32",')
    elif platform.system() == "Darwin":
        lines.append('      "name": "Apple",')
    elif platform.system() == "Linux":
        lines.append('      "name": "Linux",')

    lines.append('      "includePath": [')
    for i in range(len(includes) - 1):
        lines.append('        "' + includes[i] + '",')
    lines.append('        "' + includes[-1] + '"')
    lines.append('      ],')
    lines.append('      "defines": [')
    for i in range(len(defines) - 1):
        lines.append('        "' + defines[i] + '",')
    lines.append('        "' + defines[-1] + '"')
    lines.append('      ],')
    lines.append('      "cStandard": "c11",')

    if platform.system() == "Windows":
        lines.append('      "windowsSdkVersion": "10.0.19041.0",')
        lines.append('      "intelliSenseMode": "windows-msvc-x64"')
    elif platform.system() == "Darwin":
        lines.append('      "intelliSenseMode": "macos-clang-arm64"')
    elif platform.system() == "Linux":
        lines.append('      "intelliSenseMode": "linux-gcc-x64"')

    lines.append('    }')
    lines.append('  ]')
    lines.append('}')
    
    for i in range(len(lines)):
        lines[i] = lines[i] + "\n"
    file.writelines(lines)

with open('../.vscode/settings.json', 'w') as file:
    lines = []
    lines.append('{')
    lines.append('  "files.associations": {')
    lines.append('    "pl_*.h": "c",')
    lines.append('    "pl_*.m": "objective-c",')
    lines.append('    "pl_*.inc": "c"')
    lines.append('   },')
    lines.append('  "python.analysis.extraPaths": ["./build"]')
    lines.append('}')
    
    for i in range(len(lines)):
        lines[i] = lines[i] + "\n"
    file.writelines(lines)
