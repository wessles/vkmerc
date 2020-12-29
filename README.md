
# 🖖 `vkmerc`

![The Cascaded Shadowmapping Demo](https://i.imgur.com/RyeCnPB.png)

This is a `C++20`  / `Vulkan 1.2` renderer. This has been my primary personal project since August 2020.

## Features / Roadmap

**Completed**  
✅ Render Graph  
✅ 2 tier shader-caching with hot-reloading support  
✅ Material System w/ Shader Reflection using SPIRV-Cross  
✅ Physically Based Rendering  
✅ Cubemap filtering suite for IBL  
✅ GLTF 2.0 Model Loading  
✅ .OBJ Model / .MTL Material Loading  
✅ Cascaded Shadow-maps  
✅ Bloom Post-processing effect  
✅ Tracy Profiler implementation  
✅ Deferred Rendering  
✅ SSAO
✅ `Imgui` for in-demo user interfaces  

**Soon**  
⬜ HBAO  
⬜ HDR  
⬜ Poisson Filtering of Shadowmaps  
⬜ Temporal Antialiasing  

**Later**  
⬜ Switch from push-constant transforms to SSBO for per-object data  
⬜ Implement GPUOpen's VulkanMemoryAllocator  
⬜ Additional abstractions for descriptors in Material system  
⬜ Add better support for Linux and Mac  
⬜ Add `KHR_raytracing` functionality  
⬜ Implement support for sandboxed scripting using `Typescript`  
⬜ Serialized format to represent scenes, materials, etc. (similar to .scene files in Unity)  
⬜ Dedicated scene editor program  

## Implementation Details

### Render Graph

The render-graph system automates the verbose process of creating Framebuffers, Render Passes, Sub-passes and  Attachments. It uses a three step process:
1. Specify a **`RenderGraphSchema`**, an immutable struct which defines a directed acyclic graph, where nodes are Render Passes and edges are Attachments. Here, the properties of each pass and attachment are explicitly defined. For an attachment, this could mean format, sample-count, whether it is the swap-chain, a depth/stencil attachment, etc. This is also where you pass a function into each PassSchema which specifies the commands that are to be recorded into command buffers (`scene.draw()`, that kind of thing). 
2. Use the `RenderGraphSchema` to create a  **`RenderGraph`**. In `RenderGraph`, there are two halves: `createLayouts()`/`destroyLayouts()` and `createInstances()`/`destroyInstances()`.
- *RG Layouts* are defined as resources that do not depend on the swap-chain, so if the window is resized these objects don't need to change: Render Passes, Descriptor Set Layouts, and Pipeline Layouts.  
- *RG Instances* are objects that depend on changes to the swap-chain, and must be recreated when the window resizes: Attachment Images, Attachment Image Views, Descriptor Sets, and Framebuffers.
3. Use `RenderGraph.render(VkCommandBuffer, uint32_t)` to record the graph's commands to a CommandBuffer for rendering.
- The RenderGraph renders a scene using this pseudocode:
```
bind scene descriptor set, set=0
for each RenderPass:
	bind pass descriptor set, set=1
	pass.schema.commands(swapIndex, commandBuf)
```

---

By default, the Render Graph inserts a descriptor set at slot index 1, with all input samplers that are needed for the Pass. Alternatively, you can enable `AttachmentSchema.isInputAttachment` to omit the input sampler for an attachment, and use Vulkan's Input Attachment functionality.

You can gain some insight into how the schema works by looking at `RenderGraph.h`.

⭐ This system allowed me to fully implement ImGUI into the engine just 20 minutes.

Right now, this system highly explicit and requires a lot of user input. If the user makes an error it requires some non-trivial debugging. In the future I want to add a kind of validation layer onto the `RenderGraphSchema`, which enforces rigid constraints onto the schema. From there I could add some utility functions that make building a schema less verbose.

### Material System
Materials are an abstraction for `VkPipeline`. I bundle the extremely verbose `VkPipelineCreateInfo` and its associated structs in the `MaterialInfo` struct. There are *a lot* of options in it, including shader stages, descriptor layouts, rasterizer settings, depth testing, color blending, and more. Once you've created a `Material`, you can create a `MaterialInstance`, which contains a Descriptor Set that you can begin pushing uniforms / samplers into. You can then bind the `Material` to set the pipeline/layout, and `MaterialInstance` to bind instance-specific descriptors. After that, any mesh you draw will use the Material.

I use SPIRV-Cross to get reflection data on shaders that I compile. This way I can use descriptors in shaders without tediously maintaining a Descriptor Set Layout in my code.

### Shader Caching / Hot Reloading
I've created a **2-tier shader cache**, which supports **hot-reloading**.

To load a shader, all you need to do is query `VulkanDevice.ShaderCache.get(ShaderVariant)`, where `ShaderVariant` is a combination of the shader filename, and a list of macros.

![](https://i.imgur.com/lMUM5pc.png)

If you want to enable hot-reloading, just `VulkanDevice.ShaderCache.hotReloadCheck()` right before recording the frame's command buffer. This is done automatically by default, so long as `BaseEngine.shaderHotReloadEnabled`. Pro-tip: avoid enabling this in production, it's filesystem heavy.

![](https://i.imgur.com/U2daOUT.png)

#### `#include` macros

`#include` is fully supported by the shader caching system. In fact, the cache invalidation algorithm takes dependencies into account. This means that, if you `#include "cascades.glsl"` in `blit.frag`, and you change `cascades.glsl`, then `blit.frag` will be recompiled automatically. 

#### `.spv` filenames

Different variants of the same shader have different compilation targets. For example, if we had a shader variant for `blit.frag` with macro `FUNKY_COLORS` set to `true`, the compiled spv file would be `blit.frag.fec34a2511.spv`. If we changed the macros at all, we would get a different hash.

#### Future

I'm very happy with how the system turned out. It's been incredibly useful and fault-tolerant so far. The only weakness is that I do not account for changes to descriptor sets very gracefully. I could also make a few optimizations: reduce unnecessary file reads, decrease polling frequency, put the wait-heavy cache-validation stage on a separate thread, etc. For now, the overhead is negligible enough that I can ignore these issues.

### Physically Based Rendering
My physically based rendering shaders are found in `pbr.frag` and `pbr.vert`. The approach is quite standard: it has an albedo, roughness, metallic and emissive layer. I use a GGX normal distribution, Smith geometry function and a Schlick Fresnel approximation. The details are largely drawn from the PBR chapter of *Realtime Rendering 4e*, and [learnopengl.com](https://learnopengl.com/PBR/Theory).

I generate the UE4 BRDF lookup table and the image-based lighting (irradiance / specular) cube-maps at runtime, as seen in `base/pbr/`. I took a lot of inspiration from [Sascha Willem's GLTF Vulkan example](https://github.com/SaschaWillems/Vulkan). This is also where I got a lot of information on importing GLTF files for Vulkan.

The results of this system are visually appealing, but I would like to refactor this code in the future.

### Cascaded Shadowmaps
[My implementation of cascaded shadowmaps is explained intuitively in this video.](https://www.youtube.com/watch?v=u0pk1LyLKYQ)

### Descriptors
Most of my shaders require much of the same data: view/projection matrices, time, screen resolution, etc. I decided to create a Uniform buffer at (set=0, binding=0) called `SceneGlobalUniform`. The descriptor set layout for set 0 is defined by `Scene`. The idea is that Scene contains the basic descriptors that all shaders share. To create a `Material`, you must specify a scene. This way the descriptor set layouts are kept consistent across the board. The layout of the `Scene`-level descriptors are modifiable by the user, but so far I have only used this functionality to add another uniform buffer for shadowmap cascade data. Still, in theory you could fully replace `SceneGlobalUniform`, so long as you re-wrote many shaders to support it.

The descriptors are intended to be layered in order of increasing update frequency, looking something like this:
```
Set 0: binding 0 = SceneGlobalUniform, binding 1 = CascadesUniform, ...
Set 1: binding 0 = {Render graph input sampler}, ...
Set 2: binding 0 = {Material specific data}, ...
Push Constants: per-object data (Transform)
```

There is a problem with this system: storing a 64 byte `mat4` in the push-constants, while technically allowed by the specification's 128 byte limit, is highly discouraged. Push constants are convenient, but they should really be less than 32 bytes for cache-friendliness. For more info, see [AMD's presentation](http://gpuopen.com/wp-content/uploads/2016/03/VulkanFastPaths.pdf).

I plan on  implementing a system in which flexible per-object data is stored in a UBO or SSBO, which is bound once and indexed by the push-constant. At 32 bytes of PC, I have room to index 4294967296 unique objects and still have 24 bytes of room left over.

This system may sound similar to Dynamic buffers, but I am not going with those, as they have significant CPU overhead I would like to avoid.

## Tools
- I used **RenderDoc** extensively for debugging this engine. I also used it to analyze other games for inspiration. For example, I used RenderDoc to learn about shadow-map cascades as used in *Risk of Rain 2*, and created a very similar implementation.
- I used the built-in **Visual Studio profiler** to analyze the CPU-intensive parts of my program

## References
- I read [*Realtime Rendering (4th Edition)*](https://www.amazon.com/Real-Time-Rendering-Fourth-Tomas-Akenine-M%C3%B6ller/dp/1138627003) cover to cover, which taught me a lot about modern real-time rendering techniques, and made this project a whole lot easier. It's an excellent book that I still reference often. I can't recommend it enough.
- I also read Lengyel's [*Foundations of Game Engine Development (Volume 1)*](https://www.amazon.com/Foundations-Game-Engine-Development-Mathematics/dp/0985811749/), which gave me a solid understanding of the math behind rendering and games.
- [*C++ Primer 5th Edition*](https://www.amazon.com/Primer-5th-Stanley-B-Lippman/dp/0321714113) is how I re-acquainted myself with C++ after a long break from the language.
- I reference the [Vulkan specification](https://www.khronos.org/registry/vulkan/specs/) regularly to debug, and to improve the engine.
- I use [gpuinfo.org](http://gpuinfo.org/) to learn about memory limits and hardware support for Vulkan features.
- I learned a lot about Vulkan from [vulkan-tutorial.com](https://vulkan-tutorial.com/), and from the many helpful people on the official Vulkan Discord server.

## Installation
`vkmerc` uses `CMake 2.8`, but it is not cross-platform out of the box. I optimized the setup for `Visual Studio 19` on Windows. In theory the programs should all compile on Linux with a few tweaks to the `CMakeList.txt`. All libraries are bundled (`GLFW`, `GLM`, and a bunch of header-only libraries). All Vulkan related libraries are linked using the `FindVulkan` function in `CMake`.

In practice, just run the installer for `Vulkan 1.2 SDK` so it shows up in your `C://` drive; the build should work out of the box on Visual Studio.

Also, if you're on Windows, **make sure to enable Developer mode**. Otherwise you can't make symlinks as a non-admin, which is required for the build process. This is because of a "security feature" enacted by Windows Vista. Thanks again Vista!
