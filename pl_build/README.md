## Pilot Light Build

### Background
The **pl-build** project is a child of the larger [Pilot Light](https://github.com/PilotLightTech/pilotlight) project. In this larger project, we do not have a "build system" per se. Instead we prefer to write batch/bash scripts that directly call the compiler. If the project was an end user product, this would be the end of it. However, this is not the case. It is meant to be easily be extended through adding additional extensions and being used as a "pilot light" to start new projects. With this comes a couple issues. Extensions are meant to be cross platform so users need the ability to easily add new binaries for all target platforms with minimal duplication. Users shouldn't need to be bash or batch scripting experts to build new targets for all platforms and shouldn't need to test the build scripts continuously on each platform. 


### Requirements
* minimize duplicated information
* fine-grained control over compilation settings
* support unity builds
* support hot reloading
* generate scripts that can be used separately
* easily extended
* easy to add new platforms and compilers
* extremely light weight
* easy to customize and extended
* no preference on editor/IDE
* doesn't pretend different platforms don't exist
* entire build system can be understood in an hour

Another way of putting it, is we want to focus on what matters to build binaries. Ultimately this is just compiler & linker settings. We don't want to think about the differences in bash/batch syntax.

### Benefits Over Raw Scripts
Prior to this system, when we were writing scripts directly, we did what every developer does and began overengineering the build scripts to improve things like information duplication, configuration options, etc. However, this led to more compilicated scripts that were harder to debug and heavily deviated between platforms since batch/command are very different in features and syntax. It became hard for newcomers to work on and encouraged differences to creep in between the way individual build scripts worked.

Using pl-build allows the clever/overengineered solutions to be accomplished at a higher level by the user in python. In the end, build scripts are output that are very straight forward and much easier to debug. It also makes it easier to implement new backends to target new compilers, platforms, scripting languages, etc.

### Alternatives

Keep in mind, **we don't want a build system**. We don't want to hide away details or abstract away control. We ultimately just need a tool to assist in generating the scripts for each platform.

What about other CMake (and others)? They are bloated over complicated messes. They are also overkill for what we need (see Requirements section above).


## Documentation
Under construction.

