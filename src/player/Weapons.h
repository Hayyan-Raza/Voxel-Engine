#pragma once
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "../rendering/WeaponModels.h"

// Base class for all weapons
class Weapon {
public:
    virtual ~Weapon() = default;

    // Called every frame to handle firing and logic
    virtual void Update(float dt, bool leftClick, bool leftMouseWasPressed) = 0;

    // Called to get the model properties for rendering
    virtual GLuint GetVAO() const = 0;
    virtual int GetVertexCount() const = 0;

    virtual int GetWeaponID() const = 0;
};

class HammerWeapon : public Weapon {
public:
    void Update(float dt, bool leftClick, bool leftMouseWasPressed) override;
    GLuint GetVAO() const override { return hammerHandleVAO; }
    int GetVertexCount() const override { return hammerHandleVertexCount; }
    int GetWeaponID() const override { return 0; }
};

class AK47Weapon : public Weapon {
public:
    void Update(float dt, bool leftClick, bool leftMouseWasPressed) override;
    GLuint GetVAO() const override { return ak47VAO; }
    int GetVertexCount() const override { return ak47VertexCount; }
    int GetWeaponID() const override { return 1; }
};

class DynamiteWeapon : public Weapon {
public:
    void Update(float dt, bool leftClick, bool leftMouseWasPressed) override;
    GLuint GetVAO() const override { return dynVAO; }
    int GetVertexCount() const override { return dynVertexCount; }
    int GetWeaponID() const override { return 2; }
};

class GlockWeapon : public Weapon {
public:
    void Update(float dt, bool leftClick, bool leftMouseWasPressed) override;
    GLuint GetVAO() const override { return glockVAO; }
    int GetVertexCount() const override { return glockVertexCount; }
    int GetWeaponID() const override { return 3; }
};

class ShotgunWeapon : public Weapon {
public:
    void Update(float dt, bool leftClick, bool leftMouseWasPressed) override;
    GLuint GetVAO() const override { return shotgunVAO; }
    int GetVertexCount() const override { return shotgunVertexCount; }
    int GetWeaponID() const override { return 4; }
};

// Voxel Placer is conceptually a tool rather than a weapon, 
// but fits in the slot system.
class VoxelPlacerWeapon : public Weapon {
public:
    void Update(float dt, bool leftClick, bool leftMouseWasPressed) override;
    GLuint GetVAO() const override { return 0; }
    int GetVertexCount() const override { return 0; }
    int GetWeaponID() const override { return 5; }
};

// Initialize the array of available weapons
void InitWeapons();

// Get a specific weapon by ID (0-5). Returns nullptr if not a valid class weapon (e.g. structures > 2000)
Weapon* GetWeapon(int id);
