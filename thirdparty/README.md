## Dependency Notes

### cgltf

This is a temporary dependency until I take the time to port my own
gltf loader from a previous project. This older gltf loader was written exclusively for C++ so I full rewrite for C is necessary. Until then, cgltf
has been great. We are currently using version 1.15.

[Repository](https://github.com/jkuhlmann/cgltf)

### stb

This is probably a permanent dependency. There is basically no sane reason to
remove it. 

[Repository](https://github.com/nothings/stb)

### Dear ImGui & ImPlot

These are not dependencies however, they are currently vendored since they are hardcoded into the experimental glfw backend. Once the current backends are further along to where Dear ImGui can be entirely integrated on the user side,
these will most likely be removed. We are currently using v1.92.7 of Dear ImGui and v1.0 of ImPlot

[Dear ImGui Repository](https://github.com/ocornut/imgui/tree/docking)

[ImPlot Repository](https://github.com/epezent/implot)

