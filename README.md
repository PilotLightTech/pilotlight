
<h1 align="center">
  Pilot Light
</h1>

<p align="center">A lightweight real-time application framework with minimal dependencies.</p>

<h1></h1>

<p align="center">
   <a href="https://github.com/PilotLightTech/pilotlight/actions?workflow=Build"><img src="https://github.com/PilotLightTech/pilotlight/actions/workflows/build.yml/badge.svg?branch=master" alt="build"></a>
   <a href="https://github.com/PilotLightTech/pilotlight/actions?workflow=Static%20Analysis"><img src="https://github.com/PilotLightTech/pilotlight/actions/workflows/static-analysis.yml/badge.svg?branch=master" alt="static-analysis"></a>
   <a href="https://github.com/PilotLightTech/pilotlight/actions?workflow=Tests"><img src="https://github.com/PilotLightTech/pilotlight/actions/workflows/tests.yml/badge.svg?branch=master" alt="tests"></a>
   <a href="https://pypi.org/project/pl-build/"><img src="https://img.shields.io/pypi/v/pl-build?label=pl-build" alt="PYPI"></a>
</p>

<p align="center">
  <a href="#information">Information</a> •
  <a href="#developer-notes">Developer Notes</a> • 
  <a href="#license">License</a> •
  <a href="#gallery">Gallery</a> •
  <a href="#inspiration">Inspiration</a>
</p>

<p align="center">
  <a href="https://github.com/PilotLightTech/pilotlight-assets"><img src="https://github.com/PilotLightTech/pilotlight-assets/blob/master/images/tooling2.png" alt="Tooling Image"></a>
</p>

## Information
This is currently a project I use as a starting point for various other projects. It may not be the friendliest to newcomers (yet!) but that will improve with time as higher level extensions are created and documentation improves. More information can be found in the [wiki](https://github.com/PilotLightTech/pilotlight/wiki#what-is-pilot-light). 

## Developer Notes
Information for developers can be found in the [wiki](https://github.com/PilotLightTech/pilotlight/wiki). This includes:
* [building](https://github.com/PilotLightTech/pilotlight/wiki/Building)
* [contributing](https://github.com/PilotLightTech/pilotlight/wiki/Contributing)
* [style guide](https://github.com/PilotLightTech/pilotlight/wiki/Style-Guide)

A template application can be found [here](https://git.pilotlight.tech/pilotlight/pl-template).

## Folder Structure
* <ins>dependencies</ins> - Contains any third party libraries.
* <ins>docs</ins> - Contains documentation for the project.
* <ins>examples</ins> - Contains small complete examples that utilize stable APIs & extensions
* <ins>extensions</ins> - Contains extensions (most functionality is provided through these extensions).
* <ins>libs</ins> - Contains standalone "stb-style" libraries that can be used in other projects.
* <ins>pl_build</ins> - Contains a lightweight python-based build system used for this project.
* <ins>sandbox</ins> - Contains janky code used for development.
* <ins>scripts</ins> - Contains helper scripts for various things.
* <ins>shaders</ins> - Contains shader code.
* <ins>src</ins> - Contains the small core of Pilot Light.
* <ins>tests</ins> - Contains all unit & system tests.

## License
Pilot Light is licensed under the [MIT License](https://github.com/PilotLightTech/pilotlight/blob/master/LICENSE).

## Gallery

<p align="center">
  <a href="https://github.com/PilotLightTech/pilotlight"><img src="https://github.com/PilotLightTech/pilotlight-assets/blob/master/images/sponza1.PNG" alt="Low Poly Scene" width="1500"></a>
  <a href="https://github.com/PilotLightTech/pilotlight"><img src="https://github.com/PilotLightTech/pilotlight-assets/blob/master/images/bistro1.PNG" alt="Low Poly Scene" width="1500"></a>
  <a href="https://github.com/PilotLightTech/pilotlight"><img src="https://github.com/PilotLightTech/pilotlight-assets/blob/master/images/sponza0.PNG" alt="Low Poly Scene" width="1500"></a>
  <a href="https://github.com/PilotLightTech/pilotlight"><img src="https://github.com/PilotLightTech/pilotlight-assets/blob/master/images/courtyard2.PNG" alt="Low Poly Scene" width="1500"></a>
  <a href="https://github.com/PilotLightTech/pilotlight"><img src="https://github.com/PilotLightTech/pilotlight-assets/blob/master/images/spacesuit.png" alt="Space Suit" width="2553"></a>
  <a href="https://github.com/PilotLightTech/pilotlight"><img src="https://github.com/PilotLightTech/pilotlight-assets/blob/master/images/bistro2.PNG" alt="Low Poly Scene" width="1500"></a>
  <a href="https://github.com/PilotLightTech/pilotlight"><img src="https://github.com/PilotLightTech/pilotlight-assets/blob/master/images/courtyard1.PNG" alt="Low Poly Scene" width="1500"></a>
</p>

## Inspiration
This project is inspired by:
* [Omar Cornut](http://www.miracleworld.net/) & [Dear ImGui](https://github.com/ocornut/imgui).
* Casey Muratori & [Handmade Hero](https://handmadehero.org/)
* Turánszki János & [Wicked Engine](https://wickedengine.net/)
* [Sean Barrett](https://nothings.org/) & his [stb](https://github.com/nothings/stb) libraries
* _The Machinery_ before they were abducted

## SAST Tools

[PVS-Studio](https://pvs-studio.com/en/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.
