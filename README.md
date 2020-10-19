# `vkmerc`

![The Helmet Demo](https://i.imgur.com/oLvEUgt.png)

This is a `C++ 20`  / `Vulkan 1.2` renderer. It has the following features

- Render graph
- Cached shader-variant compilation system
- Physically Based Rendering
- Cubemap filtering suite for IBL
- GLTF 2.0 loading
- Cascaded Shadowmaps

## Installation

`vkmerc` uses `CMake 2.8`, but it is not cross-platform; this is made for Windows. All libraries are included (`GLFW`, `GLM`, and a bunch of header-only libraries). All Vulkan related libraries are linked using the `FindVulkan` CMake function.

In practice all you need to do is run the Vulkan SDK installer so it shows up in `C://` and it should work out of the box.
  
I assume you're using drivers that support the `KHR_raytracing` extension.

*(At time of writing, this means you must have beta Nvidia Vulkan drivers installed.)*