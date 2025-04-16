# Pilot Light Documentation

UNDER CONSTRUCTION

## Table of Contents
* [Overview](#overview)
  * [What is Pilot Light?](#what-is-pilot-light)
  * [Goals and Philosophy](#goals-and-philosophy)
* [Getting Started](#getting-started)
* [Architecture](#architecture)
  * [Core](#core)
  * [Application](#application)
  * [Extensions](#extensions)
* [Build System](#build-system)
* [Features](#features)
* [Examples](#examples)
* [Support](#support)
* [Change Log](#changelog)
* [License](#License)

## Overview

The Pilot Light project is a lightweight framework designed to facilitate efficient software development, in particular for real-time applications.

### What is Pilot Light?
I’m still not quite sure what label to put on Pilot Light. Calling it a “game engine” is way too generous at this stage. Maybe the best way to understand it is by describing the individual components that comprise it, goals I wish to accomplish with it, and the problems it attempts to solve.

### Goals and Philosophy

As the name suggests (Pilot Light), the project is meant to be a starting point for other projects. Whether those projects are games, tools, or prototypes, the goal is to be a useful starting point. In addition to this, I want to keep Pilot Light lightweight, performant, and (nearly) dependency free. I do not want to be too restrictive or force users to work at any specific layer of abstraction. I also want things to be as modular as possible.

In the ideal situation, everything would be [STB style](https://github.com/nothings/stb) libraries which are about as decoupled as things can be. In reality, this obviously isn't possible for everything, but the effort is still made when possible, and those libraries end up in the [libs](https://github.com/PilotLightTech/pilotlight/tree/master/libs) folder. These are standalone libraries that can easily be dropped into any project. Everything else becomes an extension.

## Getting Started

Follow the instructions found [here](https://github.com/PilotLightTech/pilotlight/wiki/Building).

Then start experimenting with the [examples](https://github.com/PilotLightTech/pilotlight/tree/master/examples).

## Architecture
The overall architecture takes inspiration from the now nonexistent **The Machinery** game engine. This architecture is very plugin (what I call extension) based. If something can be an extension, then it should be! Functionality is provided by **APIs** which are just structs of function pointers.

An example of one of these APIs might look something like this:

```c
typedef struct _plJobI
{
    // setup/shutdown
    void (*initialize)(uint32_t uThreadCount); // set thread count to 0 to get optimal thread count
    void (*cleanup)(void);

    // typical usage
    //   - submit an array of job descriptions and receive an atomic counter pointer
    //   - pass NULL for the atomic counter pointer if you don't need to wait (fire & forget)
    //   - use "wait_for_counter" to wait on jobs to complete and return counter for reuse
    void (*dispatch_jobs)(uint32_t uJobCount, plJobDesc*, plAtomicCounter**);

    // batch usage
    //   Follows more of a compute shader design. All jobs use the same data which can be indexed
    //   using the job index. If the jobs are small, consider increasing the group size.
    //   - uJobCount  : how many jobs to generate
    //   - uGroupSize : how many jobs to execute per thread serially (set 0 for optimal group size)
    //   - pass NULL for the atomic counter pointer if you don't need to wait (fire & forget)
    void (*dispatch_batch)(uint32_t uJobCount, uint32_t uGroupSize, plJobDesc, plAtomicCounter**);
    
    // waits for counter to reach 0 and returns the counter for reuse but subsequent dispatches
    void (*wait_for_counter)(plAtomicCounter*);
} plJobI;
```

The APIs are accessed through the API registry which has the following API:

```c
typedef struct _plApiRegistryI
{
    const void* (*set_api)   (const char* name, plVersion, const void* api, size_t interfaceSize);
    const void* (*get_api)   (const char* name, plVersion);
    void        (*remove_api)(const void* api);
} plApiRegistryI;

// some of the helper macros
#define pl_set_api(ptApiReg, TYPE, ptr)
#define pl_get_api_latest(ptApiReg, TYPE)
```

The APIs are provided by the core runtime and by extensions.

### Core
The core of the engine is very small and entirely contained in the [src](https://github.com/PilotLightTech/pilotlight/tree/master/src) folder. The core is just an executable that provides the following systems:
* API Registry
* Data Registry
* Extension Registry

It also provides APIs for those systems in addition to APIs for general memory allocation, keyboard/mouse input, etc. These APIs are required by all platform backends and are found in [pl.h](https://github.com/PilotLightTech/pilotlight/blob/master/src/pl.h).

In addition to the above APIs and systems, the core is responsible for handling loading, unloading, and hot reloading of applications and extensions.

### Application
An application is just a dynamic library that exports the following functions:
```c
PL_EXPORT void* pl_app_load    (plApiRegistryI*, void*);
PL_EXPORT void  pl_app_shutdown(void*);
PL_EXPORT void  pl_app_resize  (plWindow*, void*);
PL_EXPORT void  pl_app_update  (void*);
PL_EXPORT bool  pl_app_info    (plApiRegistryI*); // optional
```
Applications are hot reloadable. A very minimal app can be seen in [example_basic_0.c](https://github.com/PilotLightTech/pilotlight/blob/master/examples/example_basic_0.c).

### Extensions
An extension is just a dynamic library that exports the following functions:
```c
PL_EXPORT void pl_load_ext  (plApiRegistryI*, bool reload);
PL_EXPORT void pl_unload_ext(plApiRegistryI*, bool reload);
```

## Build System

The project does not have a typical build system. We prefer to just use plain batch/bash scripts that directly call the compiler. However, this starts to be hard to maintain when supporting multiple platforms and target binaries. It can also be error prone. So I built our own little build system (called pl-build). It is entirely contained in the [pl_build](https://github.com/PilotLightTech/pilotlight/tree/master/pl_build) folder. It is standalone and can also be found on [pypi](https://pypi.org/project/pl-build/). It is python based. It simply outputs the final build scripts for each platform. You can see how we use it in the [scripts](https://github.com/PilotLightTech/pilotlight/tree/master/scripts) folder (gen_*.py files).

## Features

A brief list of broad features available can be found in [features.txt](https://github.com/PilotLightTech/pilotlight/blob/master/docs/features.txt).

## Examples

Examples can be found [here](https://github.com/PilotLightTech/pilotlight/tree/master/examples).

## Support

- **Issue Tracker**
  - To report bugs or request features go [here](https://github.com/PilotLightTech/pilotlight/issues).
- **Contributing**
  - Guidelines for contributing code or documentation can be found [here](https://github.com/PilotLightTech/pilotlight/wiki/Contributing).

## Changelog

See [changelog.txt](https://github.com/PilotLightTech/pilotlight/blob/master/docs/changelog.txt).

## License
Pilot Light is under the [MIT License](https://github.com/PilotLightTech/pilotlight/blob/master/LICENSE).

---

*This documentation is a living resource and will be updated as Pilot Light evolves.*