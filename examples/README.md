## Examples
The following examples have been provided to help newcomers become acclimated to using **Pilot Light** extensions. The examples can be found in the **examples** directory.

To run the examples, first follow the build instructions [here](https://github.com/PilotLightTech/pilotlight/wiki/Building).

Next:

### Windows
Example for running example 0:
```bash
cd src
build_win32.bat
cd ../examples
build_win32.bat

cd ../out
pilot_light.exe -a example_basic_0
```
### Linux
Example for running example 2:
```bash
cd src
chmod +x build_linux.sh
./build_linux.sh
cd ../examples
chmod +x build_linux.sh
./build_linux.sh

cd ../out
pilot_light -a example_gfx_2
```

## Basic Examples

### Example 0 - Minimal App With API Registry  (example_basic_0.c)
Demonstrates the bare minimum app. This app loads, runs 50 iterations of the update function (printing to console), then exits. Note: this app is not really meant to run. It is for reference.

### Example 1 - API Loading With API Registry  (example_basic_1.c)
Note: this app is not really meant to run. It is for reference.
Demonstrates:
* loading APIs
* hot reloading

### Example 2 - Starter & Basic Extensions (example_basic_2.c)
Demonstrates:
* loading extensions
* starter extension
* basic drawing extension (2D)
* basic screen log extension
* basic console extension
* basic UI extension

### Example 3 - Starter & Basic Extensions With API Registry (example_basic_3.c)
Demonstrates:
* loading APIs
* loading extensions
* starter extension
* basic drawing extension (2D)
* basic screen log extension
* basic console extension
* basic UI extension

### Example 4 - Draw Extension With API Registry (example_basic_4.c)
Demonstrates:
* loading APIs
* loading extensions
* drawing extension (2D)

### Example 5 - UI Extension With API Registry (example_basic_5.c)
Demonstrates:
* loading APIs
* loading extensions
* hot reloading
* ui extension

### Example 6 - Dear ImGui With API Registry (example_basic_6.c)
Demonstrates:
* Dear ImGui integration


## Low Level Graphics Examples

### Example 0 - Graphics Extension 0 With API Registry (example_gfx_0.c)
Demonstrates:
* vertex buffers
* shaders
* non-index drawing

### Example 1 - Graphics Extension 1 With API Registry (example_gfx_1.c)
Demonstrates:
* vertex buffers
* index buffers
* staging buffers
* shaders
* indexed drawing

### Example 2 - Graphics Extension 2 With API Registry (example_gfx_2.c)
Demonstrates:
* bind groups
* vertex, index, staging buffers
* samplers, textures, bind groups
* shaders
* indexed drawing
* image extension

### Example 3 - Graphics Extension 3 With API Registry (example_gfx_3.c)
Demonstrates:
* starter extension
* drawing extension (2D & 3D)

### Example 4 - Graphics Extension 4 With API Registry (example_gfx_4.c)
Demonstrates:
* starter extension
* render passes
