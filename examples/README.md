## Examples
The following examples have been provided to help newcomers become acclimated to using **Pilot Light**. The examples can be found in the **examples** directory.

To run the examples, first follow the build instructions [here](https://github.com/PilotLightTech/pilotlight/wiki/Building).

Next:

### Windows
Example for running example 0:
```bash
cd pilotlight/tools
python gen_examples.py
cd ../examples
build.bat

cd ../out
pilot_light.exe -a example_0
```
### MacOS & Linux
Example for running example 0:
```bash
cd pilotlight/tools
python3 gen_examples.py
cd ../examples
chmod +x build.sh
./build.sh

cd ../out
pilot_light -a example_0 
```

## Example 0 - Minimal App (example_0.c)
Demonstrates the bare minimumal app. This app loads, runs 50 iterations of the update function (printing to console), then exits.

## Example 1 - API Loading (example_1.c)
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

## Example 4 - Drawing Extension 3D (example_4.c)
Demonstrates:
* loading APIs
* hot reloading
* loading extensions
* minimal use of graphics extension
* drawing extension (3D)
