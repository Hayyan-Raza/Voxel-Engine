CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -O3 -ffast-math -march=native -w -mconsole -DGLFW_INCLUDE_NONE -DGLM_ENABLE_EXPERIMENTAL -DBT_USE_DOUBLE_PRECISION
CFLAGS = -O3 -ffast-math -march=native
INCLUDES = -Isrc -Iinclude -Iinclude/imgui -Iglad/include -IC:/msys64/ucrt64/include/bullet -IC:/msys64/ucrt64/include
LDFLAGS = -LC:/msys64/ucrt64/lib -lglfw3 -lgdi32 -lopengl32 -lBulletDynamics -lBulletCollision -lLinearMath

TARGET = TearDownClone.exe
OBJDIR = obj

SRCS = src/main.cpp \
       src/core/Engine.cpp \
       src/core/Globals.cpp \
       src/core/Audio.cpp \
       src/world/World.cpp \
       src/world/ChunkManager.cpp \
       src/world/TerrainGenerator.cpp \
       src/world/TreeGenerator.cpp \
       src/world/HouseGenerator.cpp \
       src/world/Lighting.cpp \
       src/world/GreedyMesher.cpp \
       src/physics/Physics.cpp \
       src/physics/IslandDetection.cpp \
       src/physics/Ragdoll.cpp \
       src/physics/RagdollBuilder.cpp \
       src/physics/Chick.cpp \
       src/physics/Mob.cpp \
       src/physics/CollisionSolver.cpp \
       src/physics/Raycast.cpp \
       src/world/VoxelModifier.cpp \
       src/particles/Particles.cpp \
       src/rendering/ShaderManager.cpp \
       src/rendering/Renderer.cpp \
       src/rendering/RenderPipeline.cpp \
       src/rendering/WeaponModels.cpp \
       src/rendering/PostProcess.cpp \
       src/player/Player.cpp \
       src/player/WeaponSystem.cpp \
       src/player/Weapons.cpp \
       src/input/Input.cpp \
       src/ui/UIManager.cpp \
       src/ui/DebugUI.cpp \
       src/ui/InventoryUI.cpp \
       src/ui/DialogUI.cpp \
       src/ui/ToolbarUI.cpp \
       src/ui/BuildingSystem.cpp \
       src/imgui/imgui.cpp \
       src/imgui/imgui_draw.cpp \
       src/imgui/imgui_widgets.cpp \
       src/imgui/imgui_tables.cpp \
       src/imgui/imgui_impl_glfw.cpp \
       src/imgui/imgui_impl_opengl3.cpp

C_SRCS = glad/src/gl.c

OBJS = $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SRCS)) \
       $(patsubst glad/src/%.c,$(OBJDIR)/gl.o,$(C_SRCS))

all: $(OBJDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDES) $(LDFLAGS)

$(OBJDIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/gl.o: glad/src/gl.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)/core $(OBJDIR)/world $(OBJDIR)/physics $(OBJDIR)/particles $(OBJDIR)/rendering $(OBJDIR)/player $(OBJDIR)/input $(OBJDIR)/imgui $(OBJDIR)/ui

clean:
	rm -rf $(OBJDIR) $(TARGET)

run: all
	./$(TARGET)
