@echo off
set "PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%"

:: Kill any existing game process
taskkill /F /IM TearDownClone.exe /T 2>nul

echo ======== Building with Direct G++ ========

:: Source files
set "SOURCES=src/main.cpp src/World.cpp src/imgui.cpp src/imgui_draw.cpp src/imgui_widgets.cpp src/imgui_tables.cpp src/imgui_impl_glfw.cpp src/imgui_impl_opengl3.cpp glad/src/gl.c"

:: Includes and Libs (MSYS2 UCRT64 Paths)
set "INCLUDES=-Iinclude -Iinclude/imgui -Iglad/include -IC:/msys64/ucrt64/include/bullet -IC:/msys64/ucrt64/include"
set "LIBS=-LC:/msys64/ucrt64/lib -lBulletDynamics -lBulletCollision -lLinearMath -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 -lpthread -static-libgcc -static-libstdc++"

:: Compile
g++ -std=c++17 -O3 -DBT_USE_DOUBLE_PRECISION %SOURCES% -o TearDownClone.exe %INCLUDES% %LIBS%

if %ERRORLEVEL% NEQ 0 (
    echo Build Failed
    pause
    exit /b %ERRORLEVEL%
)

echo ======== SUCCESS! Build Complete ========
