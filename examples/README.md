## Examples
The following examples have been provided to help newcomers become acclimated to using **Pilot Light**. The examples can be found in the **examples** directory.

To run the examples, first follow the build instructions [here](https://github.com/PilotLightTech/pilotlight/wiki/Building).

Next:

### Windows
Example for running example 0:
```bash
cd scripts
python gen_build.py
cd ../src
build.bat
cd ../examples
build.bat

cd ../out
pilot_light.exe -a example_0
```
### Linux & MacOS
Example for running example 2:
```bash
cd scripts
python3 gen_build.py
cd ../src
chmod +x build.sh
./build.sh
cd ../examples
chmod +x build.sh
./build.sh

cd ../out
pilot_light -a example_2
```

## Example 0 - Minimal App (example_0.c)
Demonstrates the bare minimum app. This app loads, runs 50 iterations of the update function (printing to console), then exits. Note: this app is not really meant to run. It is for reference.

## Example 1 - API Loading (example_1.c)
Note: this app is not really meant to run. It is for reference.
Demonstrates:
* loading APIs
* hot reloading

## Example 2 - Drawing Extension 2D (example_2.c)
Demonstrates:
* loading APIs
* hot reloading
* loading extensions
* minimal use of graphics extension
* drawing extension (2D)

## Example 3 - UI Extension (example_3.c)
Demonstrates:
* loading APIs
* hot reloading
* loading extensions
* minimal use of graphics extension
* drawing extension (2D)
* UI extension

## Example 4 - Hello Triangle (example_4.c)
Demonstrates:
* loading APIs
* hot reloading
* loading extensions
* vertex buffers
* graphics shaders
* non-indexed drawing

## Example 5 - Hello Quad (example_5.c)
Demonstrates:
* loading APIs
* hot reloading
* loading extensions
* vertex buffers
* index buffers
* staging buffers
* graphics shaders
* indexed drawing

## Example 6 - Textured Quad (example_6.c)
Demonstrates:
* loading APIs
* hot reloading
* loading extensions
* vertex, index, staging buffers
* samplers, textures, bindgroups
* graphics shaders
* indexed drawing
* image extension

## Example 7 - Compute (example_7.c)
Demonstrates:
* WIP

## Example 8 - Drawing Extension 3D (example_8.c)
Demonstrates:
* loading APIs
* hot reloading
* loading extensions
* minimal use of graphics extension
* drawing extension (3D)
