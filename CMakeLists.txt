cmake_minimum_required(VERSION 3.10)
project(TearDownClone)

set(CMAKE_CXX_STANDARD 17)

# Find Packages
find_package(Bullet REQUIRED)
find_package(glfw3 REQUIRED)

# Include Directories
include_directories(
    src
    include
    ${BULLET_INCLUDE_DIRS}
)

# Source Files
set(SOURCES
    src/main.cpp
    glad/src/gl.c
    src/imgui.cpp
    src/imgui_draw.cpp
    src/imgui_widgets.cpp
    src/imgui_tables.cpp
    src/imgui_impl_glfw.cpp
    src/imgui_impl_opengl3.cpp
    src/rendering/Shaders.cpp
    src/rendering/Renderer.cpp
    src/rendering/WeaponModels.cpp
    src/rendering/PostProcess.cpp
    src/world/World.cpp
    src/world/TreeGenerator.cpp
    src/world/GreedyMesher.cpp
    src/physics/Physics.cpp
    src/physics/IslandDetection.cpp
    src/physics/Ragdoll.cpp
    src/particles/Particles.cpp
    src/input/Input.cpp
    src/ui/BuildingSystem.cpp
    src/player/Player.cpp
    src/core/Globals.cpp
    src/core/Audio.cpp
    src/physics/Mob.cpp
)

# Executable
add_executable(TearDownClone ${SOURCES})

# Links
target_link_libraries(TearDownClone
    BulletDynamics
    BulletCollision
    LinearMath
    glfw
    opengl32
    gdi32
)
