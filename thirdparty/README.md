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

These are not dependencies however, they are currently vendored since we use our own backend. Newer versions should work as well.

[Dear ImGui Repository](https://github.com/ocornut/imgui/tree/docking)

[ImPlot Repository](https://github.com/epezent/implot)

