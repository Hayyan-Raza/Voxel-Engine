$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH

$sources = @(
    "src/main.cpp",
    "src/core/Globals.cpp",
    "src/core/Audio.cpp",
    "src/world/World.cpp",
    "src/world/TreeGenerator.cpp",
    "src/world/HouseGenerator.cpp",
    "src/world/GreedyMesher.cpp",
    "src/physics/Physics.cpp",
    "src/physics/IslandDetection.cpp",
    "src/physics/Mob.cpp",
    "src/physics/Ragdoll.cpp",
    "src/particles/Particles.cpp",
    "src/rendering/ShaderManager.cpp",
    "src/rendering/Renderer.cpp",
    "src/rendering/WeaponModels.cpp",
    "src/rendering/PostProcess.cpp",
    "src/player/Player.cpp",
    "src/imgui/imgui.cpp",
    "src/imgui/imgui_draw.cpp",
    "src/imgui/imgui_widgets.cpp",
    "src/imgui/imgui_tables.cpp",
    "src/imgui/imgui_impl_glfw.cpp",
    "src/imgui/imgui_impl_opengl3.cpp",
    "glad/src/gl.c"
)

Write-Host "======== Building TearDown Engine ========"

# Includes
$includes = "-Isrc", "-Iinclude", "-Iinclude/imgui", "-Iglad/include", "-IC:/msys64/ucrt64/include/bullet", "-IC:/msys64/ucrt64/include"

# Defines
$defines = "-DGLFW_INCLUDE_NONE", "-DGLM_ENABLE_EXPERIMENTAL", "-DBT_USE_DOUBLE_PRECISION"

# Libs
$libs = "-LC:/msys64/ucrt64/lib", "-lglfw3", "-lgdi32", "-lopengl32", "-lBulletDynamics", "-lBulletCollision", "-lLinearMath"

# Build Command
Write-Host "Compiling..."
g++ -std=c++17 -O2 -w -mconsole $defines $sources -o TearDownClone.exe $includes $libs

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build Successful! Executable: TearDownClone.exe" -ForegroundColor Green
} else {
    Write-Host "Build Failed!" -ForegroundColor Red
}
