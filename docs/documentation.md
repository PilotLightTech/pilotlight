# Pilot Light Documentation

## Overview

The Pilot Light project is a lightweight framework designed to facilitate efficient software development, in particular for real-time applications.

As the name suggests (Pilot Light), the project is meant to be a starting point for other projects. Whether those projects are games, tools, or prototypes, the goal is to be a useful starting point. In addition to this, I want to keep Pilot Light lightweight, performant, and (nearly) dependency free. I do not want to be too restrictive or force users to work at any specific layer of abstraction. I also want things to be as modular as possible.

In the ideal situation, everything would be [STB style](https://github.com/nothings/stb) libraries which are about as decoupled as things can be. In reality, this obviously isn't possible for everything, but the effort is still made when possible, and those libraries end up in the [libs](https://github.com/PilotLightTech/pilotlight/tree/master/libs) folder. These are standalone libraries that can easily be dropped into any project. Everything else becomes an extension.

- **What is Pilot Light?**
  - High-level description of the engine.
  - Target audience.
- **Core Features**
  - List key features.
- **Goals and Philosophy**
  - Design principles or vision behind Pilot Light.

## Getting Started
Guides for users to quickly set up and start using the engine.

- **System Requirements**
  - Hardware and software prerequisites.
- **Installation**
  - Step-by-step instructions for downloading and installing.
- **First Project**
  - Tutorial to create a simple project (e.g., a basic scene or game loop).
- **Directory Structure**
  - Explanation of the engine’s file organization.

## Architecture
The overall architecture takes inspiration from the now nonexistent **The Machinery** game engine. This architecture is very plugin (what I call extension) based. If something can be an extension, then it should be! Functionality is provided by **APIs** which are just structs of function pointers.

- **Core Components**
  - Breakdown of major systems (e.g., rendering, input, physics).
- **Entity-Component-System (ECS)**
  - Describe the ECS architecture.
- **Data Flow**
  - How data moves between systems (e.g., input to rendering).
- **Extensibility**
  - How users can extend or customize the engine.

## Features
Detailed documentation for each major feature or module.

- **Rendering**
  - Supported graphics APIs
  - Shaders, materials, and lighting.
- **Physics**
  - Collision detection, rigid bodies, physics integration, etc.
- **Audio**
  - Sound effects, music, and spatial audio.
- **Scripting**
  - Supported languages.
  - How to write and integrate scripts.
- **UI System**
  - Tools for creating user interfaces.
- **Networking**
  - Networking information.
- **Asset Pipeline**
  - Importing and managing assets (models, textures, etc.).

## Tools and Editors
Information about built-in or companion tools for Pilot Light.

- **Scene Editor**
  - How to create and manage scenes.
- **Asset Editor**
  - Tools for editing assets or creating content.
- **Debugging Tools**
  - Profilers, log viewers, or other utilities.
- **Command-Line Interface (CLI)**
  - If applicable, for automation or builds.

## Tutorials & Examples

Examples can be found [here](https://github.com/PilotLightTech/pilotlight/tree/master/examples).

## API Reference
Technical documentation for developers integrating with Pilot Light.

- **Core Classes/Functions**
  - Key classes or functions with descriptions.
- **Scripting API**
  - Detailed reference for scripting functionality.
- **Configuration Files**
  - Format and options for config files.

## Best Practices
Guidelines for getting the most out of Pilot Light.

- **Project Organization**
  - Recommended folder structures and naming conventions.
- **Performance Tips**
  - Common pitfalls and optimization strategies.
- **Memory Management**
  - How to handle resources efficiently.

## Troubleshooting
Common issues and their solutions.

- **Installation Issues**
  - Fixes for setup problems.
- **Runtime Errors**
  - Debugging crashes or unexpected behavior.
- **Performance Bottlenecks**
  - Identifying and resolving slowdowns.

## Support

- **Issue Tracker**
  - To report bugs or request features go [here](https://github.com/PilotLightTech/pilotlight/issues).
- **Contributing**
  - Guidelines for contributing code or documentation can be found [here](https://github.com/PilotLightTech/pilotlight/wiki/Contributing).

## Changelog
History of updates to Pilot Light.

- **Version X.Y.Z**
  - Release date, new features, bug fixes, and breaking changes.

## License
Pilot Light is under the [MIT License](https://github.com/PilotLightTech/pilotlight/blob/master/LICENSE).

---

*This documentation is a living resource and will be updated as Pilot Light evolves.*