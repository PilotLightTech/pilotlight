import os
import platform

########################################################################################################################
# setup vs code
########################################################################################################################

if not os.path.isdir('.vscode'):
    os.mkdir('.vscode')

if(platform.system() == "Windows"):

    with open('.vscode/launch.json', 'w') as file:
        file.write(
"""{
    // Use IntelliSense to learn about possible attributes.
    // Hover to view descriptions of existing attributes.
    // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
    "version": "0.2.0",
    "configurations": [
        {
            "name": "(Windows) Launch",
            "type": "cppvsdbg",
            "request": "launch",
            "program": "${workspaceFolder}/out/pilot_light.exe",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}/out/",
            "environment": [],
            "console": "internalConsole"
        }
    ]
}
""")

    with open('.vscode/c_cpp_properties.json', 'w') as file:
        file.write(
"""{
    "configurations": [
        {
            "name": "Win32",
            "includePath": [
                "${workspaceFolder}/**",
                "${workspaceFolder}/dependencies/stb",
                "${env:VK_SDK_PATH}/Include"
            ],
            "defines": [
                "_DEBUG",
                "PL_PROFILE_ON",
                "PL_LOG_IMPLEMENTATION",
                "PL_PROFILE_IMPLEMENTATION",
                "PL_LOG_ON",
                "VULKAN_PL_DRAWING_IMPLEMENTATION"
            ],
            "windowsSdkVersion": "10.0.19041.0",
            "cStandard": "c99",
            "intelliSenseMode": "windows-msvc-x64"
        }
    ],
    "version": 4
}
""")

elif(platform.system() == "Darwin"):

    with open('.vscode/launch.json', 'w') as file:
        file.write(
"""{
    // Use IntelliSense to learn about possible attributes.
    // Hover to view descriptions of existing attributes.
    // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
    "version": "0.2.0",
    "configurations": [
        {
            "name": "(lldb) Launch",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/out/pilot_light",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}/out/",
            "environment": [],
            "externalConsole": false,
            "MIMode": "lldb"
        }
    ]
}
""")

    with open('.vscode/c_cpp_properties.json', 'w') as file:
        file.write(
"""{
    "configurations": [
        {
            "name": "Apple",
            "includePath": [
                "${workspaceFolder}/**",
                "${workspaceFolder}/dependencies/stb",
            ],
            "defines": [
                "_DEBUG",
                "PL_LOG_IMPLEMENTATION",
                "PL_PROFILE_IMPLEMENTATION",
                "PL_LOG_ON",
                "PL_PROFILE_ON",
                "METAL_PL_DRAWING_IMPLEMENTATION",
                "VULKAN_PL_DRAWING_IMPLEMENTATION"
            ],
            "cStandard": "c99",
            "intelliSenseMode": "macos-clang-arm64"
        }
    ],
    "version": 4
}
""")

elif(platform.system() == "Linux"):

    with open('.vscode/launch.json', 'w') as file:
        file.write(
"""{
    // Use IntelliSense to learn about possible attributes.
    // Hover to view descriptions of existing attributes.
    // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
    "version": "0.2.0",
    "configurations": [
        {
            "name": "(lldb) Launch",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/out/pilot_light",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}/out/",
            "environment": []
        }
    ]
}
""")

    with open('.vscode/c_cpp_properties.json', 'w') as file:
        file.write(
"""{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/**",
                "${workspaceFolder}/dependencies/stb",
            ],
            "defines": [
                "_DEBUG",
                "PL_LOG_IMPLEMENTATION",
                "PL_PROFILE_IMPLEMENTATION",
                "PL_LOG_ON",
                "PL_PROFILE_ON",
                "VULKAN_PL_DRAWING_IMPLEMENTATION"
            ],
            "cStandard": "c99",
            "intelliSenseMode": "linux-gcc-x64"
        }
    ],
    "version": 4
}
""")