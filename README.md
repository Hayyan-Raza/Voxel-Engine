# Voxel-Engine (TearDown Clone)

A high-performance, multithreaded C++ voxel engine featuring infinite procedural terrain, dynamic destruction, and physics simulation.

## Screenshot



![alt text](image.png)

## Features

- **Infinite Procedural Terrain**: Infinite world generation driven by Perlin noise, featuring varied terrain, trees, grass, flowers, and natural structures.
- **Asynchronous Chunk Paging**: Lightning-fast, multithreaded chunk loading and saving that offloads disk I/O and generation to background workers for stutter-free exploration.
- **Greedy Meshing**: Highly optimized chunk mesh generation that groups adjacent identical voxels to drastically reduce vertex count and rendering overhead.
- **Dynamic Physics & Destruction**: Integrated with **Bullet Physics** to support voxel raycasting, structural damage, and rigid body dynamics.
- **Multithreading Architecture**: Smartly balances CPU cores across chunk generation, meshing, and saving pipelines for maximum parallel throughput.
- **Voxel Modifying Tools**: Equip weapons/tools (like the hammer) to break or build blocks dynamically in real-time.
- **Rendering & Particles**: Custom OpenGL rendering pipeline featuring ambient occlusion (AO), custom shaders, and particle systems.
- **Debug UI**: Integrated **Dear ImGui** interface to tweak engine parameters, rendering options, and weapon positions on the fly.

## Dependencies

This engine requires the following libraries (typically included or linked in the build system):
- **OpenGL**
- **GLFW** - Window creation and input handling
- **GLAD** - OpenGL function loading
- **GLM** - OpenGL Mathematics
- **Bullet3** - Physics engine
- **Dear ImGui** - Immediate mode GUI

## Build Instructions (Windows)

A `build.bat` script and a `Makefile` are provided for compilation on Windows using `g++` (MinGW).

1. Clone the repository:
   ```bash
   git clone https://github.com/Hayyan-Raza/Voxel-Engine.git
   cd Voxel-Engine
   ```

2. Run the build script:
   ```cmd
   build.bat
   ```
   *(Alternatively, you can run `make` if you have Make installed).*

3. Run the compiled executable:
   ```cmd
   ./TearDownClone.exe
   ```

## Controls
- **W, A, S, D**: Move
- **Space**: Jump
- **Mouse**: Look around
- **Left Click**: Use equipped tool (break voxels / interact)
- **Right Click**: Build voxels / Secondary action
- **Esc / F1**: Toggle Debug Menu
