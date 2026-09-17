@echo off
set "PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%"

:: Kill any existing game process
taskkill /F /IM TearDownClone.exe /T 2>nul

echo ======== Building TearDown Engine ========

set SOURCES=src/main.cpp src/core/Audio.cpp src/core/Engine.cpp src/core/Globals.cpp src/imgui/imgui.cpp src/imgui/imgui_draw.cpp src/imgui/imgui_impl_glfw.cpp src/imgui/imgui_impl_opengl3.cpp src/imgui/imgui_tables.cpp src/imgui/imgui_widgets.cpp src/input/Input.cpp src/particles/Particles.cpp src/physics/Chick.cpp src/physics/CollisionSolver.cpp src/physics/IslandDetection.cpp src/physics/Mob.cpp src/physics/Physics.cpp src/physics/Ragdoll.cpp src/physics/RagdollBuilder.cpp src/physics/Raycast.cpp src/player/Player.cpp src/player/Weapons.cpp src/player/WeaponSystem.cpp src/rendering/PostProcess.cpp src/rendering/Renderer.cpp src/rendering/RenderPipeline.cpp src/rendering/ShaderManager.cpp src/rendering/WeaponModels.cpp src/ui/BuildingSystem.cpp src/ui/DebugUI.cpp src/ui/DialogUI.cpp src/ui/InventoryUI.cpp src/ui/ToolbarUI.cpp src/ui/UIManager.cpp src/world/ChunkManager.cpp src/world/GreedyMesher.cpp src/world/HouseGenerator.cpp src/world/Lighting.cpp src/world/TerrainGenerator.cpp src/world/TreeGenerator.cpp src/world/VoxelModifier.cpp src/world/WaterSimulator.cpp src/world/World.cpp glad/src/gl.c

:: === Includes ===
set INCLUDES=-Isrc -Iinclude -Iinclude/imgui -Iglad/include -IC:/msys64/ucrt64/include/bullet -IC:/msys64/ucrt64/include

:: === Libs ===
set LIBS=-LC:/msys64/ucrt64/lib -lBulletDynamics -lBulletCollision -lLinearMath -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 -lpthread -static-libgcc -static-libstdc++

:: === Compile ===
g++ -std=c++17 -O2 -mconsole -DGLM_ENABLE_EXPERIMENTAL -DGLFW_INCLUDE_NONE -DBT_USE_DOUBLE_PRECISION %SOURCES% -o TearDownClone.exe %INCLUDES% %LIBS%

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ======== Build FAILED ========
    exit /b %ERRORLEVEL%
)

echo ======== SUCCESS! Build Complete ========
