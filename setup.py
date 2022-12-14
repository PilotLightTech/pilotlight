import os
import platform

defines = [
    "_DEBUG",
    "PL_PROFILE_ON",
    "PL_LOG_IMPLEMENTATION",
    "PL_MEMORY_IMPLEMENTATION",
    "PL_PROFILE_IMPLEMENTATION",
    "PL_REGISTRY_IMPLEMENTATION",
    "PL_LOG_ON",
    "PL_IO_IMPLEMENTATION",
    "PL_MEMORY_IMPLEMENTATION",
    "PL_DRAW_DX11_IMPLEMENTATION",
    "PL_EXT_IMPLEMENTATION",
    "PL_CAMERA_IMPLEMENTATION",
    "PL_STL_EXT_IMPLEMENTATION",
    "PL_DRAW_VULKAN_IMPLEMENTATION"
]

includes = [
    "${workspaceFolder}/**",
    "${workspaceFolder}/src",
    "${workspaceFolder}/extensions",
    "${workspaceFolder}/dependencies/stb",
    "${workspaceFolder}/dependencies/cgltf",
    "${env:VK_SDK_PATH}/Include"
]

########################################################################################################################
# setup vs code
########################################################################################################################

if not os.path.isdir('.vscode'):
    os.mkdir('.vscode')

with open('.vscode/launch.json', 'w') as file:

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
        lines.append('      "program": "${workspaceFolder}/out/pilot_light",')
        lines.append('      "externalConsole": false,')
        lines.append('      "MIMode": "lldb",')

    elif platform.system() == "Linux":
        lines.append('      "name": "(lldb) Launch",')
        lines.append('      "type": "cppdbg",')
        lines.append('      "program": "${workspaceFolder}/out/pilot_light",')

    lines.append('      "request": "launch",')
    lines.append('      "args": [],')
    lines.append('      "stopAtEntry": false,')
    lines.append('      "cwd": "${workspaceFolder}/out/",')
    lines.append('      "environment": []')
    lines.append('    }')
    lines.append('  ]')
    lines.append('}')

    for i in range(len(lines)):
        lines[i] = lines[i] + "\n"
    file.writelines(lines)

with open('.vscode/c_cpp_properties.json', 'w') as file:
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

with open('.vscode/settings.json', 'w') as file:
    lines = []
    lines.append('{')
    lines.append('  "files.associations": {')
    lines.append('    "pl_*.h": "c",',)
    lines.append('    "pl_*.inc": "c"')
    lines.append('   }')
    lines.append('}')
    
    for i in range(len(lines)):
        lines[i] = lines[i] + "\n"
    file.writelines(lines)
