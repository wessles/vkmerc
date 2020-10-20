
# `vkmerc`

![The Helmet Demo](https://i.imgur.com/oLvEUgt.png)

This is a `C++ 20`  / `Vulkan 1.2` renderer. This has been my primary personal project since August 2020.

## Features / Roadmap

**Completed**  
✅ Render Graph  
✅ Material System w/ Shader Reflection using SPIRV  
✅ Cached shader-variant compilation system  
✅ Physically Based Rendering  
✅ Cubemap filtering suite for IBL  
✅ GLTF 2.0 Model Loading  
✅ .OBJ Model / .MTL Material Loading

**Soon**  
⬜ Cascaded Shadow-maps with Poisson Filtering  
⬜ Temporal Antialiasing
⬜ Implement GPUOpen's VulkanMemoryAllocator

**Later**  
⬜ Add `KHR_raytracing` functionality  
⬜ Add better support for Linux and Mac  
⬜ Implement `Imgui` for in-demo user interfaces  
⬜ Additional abstractions for descriptors in Material system  
⬜ Implement support for sandboxed scripting using `Typescript`  
⬜ Serialized format to represent scenes, materials, etc. (similar to .scene files in Unity)  
⬜ Dedicated scene editor program

## Installation

`vkmerc` uses `CMake 2.8`, but it is not cross-platform out of the box. I optimized the setup for `Visual Studio 19` on Windows. In theory the programs should all compile on Linux with a few tweaks to the `CMakeList.txt`. All libraries are bundled (`GLFW`, `GLM`, and a bunch of header-only libraries). All Vulkan related libraries are linked using the `FindVulkan` function in `CMake`.

In practice, just run the installer for `Vulkan 1.2 SDK` so it shows up in your `C://` drive; the build should work out of the box on Visual Studio.

Also, if you're on Windows, **make sure to enable Developer mode**. Otherwise you can't make symlinks as a non-admin, which is required for the build process. This is because of a "security feature" enacted by Windows Vista. Thanks again Vista!

I assume you're using drivers that support the `KHR_raytracing` extension. *(At time of writing, this means you must have beta Nvidia Vulkan drivers installed.)*
