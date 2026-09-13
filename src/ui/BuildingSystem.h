#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct ImDrawList;

namespace UI {

enum class BuildModeState {
    Inactive,
    RadialMenu_Main,
    RadialMenu_Shapes,
    RadialMenu_Prefabs,
    GizmoPreview,
    GizmoAnchored
};

enum class ShapeType {
    Sphere, 
    Cylinder, 
    Bar, 
    Pyramid, 
    Arrow, 
    Hexagon, 
    Box
};

struct RadialMenuItem {
    std::string label;
    int id;
    ShapeType shapeType; // Used if id implies a shape
};

struct RadialMenu {
    std::vector<RadialMenuItem> items;
    int hoveredIndex = -1;
    float radius = 200.0f; 
    float currentPointerAngle = 0.0f; // Angle of the center arrow
};

enum class GizmoDragState {
    None,
    OuterRadius,
    InnerRadius,
    MoveX,
    MoveY,
    MoveZ,
    ScalePX, // Positive X face
    ScaleNX, // Negative X face
    ScalePY,
    ScaleNY,
    ScalePZ,
    ScaleNZ
};

struct PlacementGizmo {
    glm::vec3 position{0.0f};
    glm::vec3 extents{5.0f}; // Replaces outer/inner radius for full 3D control
    
    GizmoDragState activeDrag = GizmoDragState::None;
    ShapeType selectedShape = ShapeType::Box;
    
    glm::vec3 dragStartWorldPos{0.0f};
    glm::vec3 dragStartExtents{0.0f};
    glm::vec3 dragStartGizmoPos{0.0f};

    std::vector<glm::vec3> previewVoxels;
    bool previewDirty = true;
    
    glm::ivec3 lastIntExtents{-1, -1, -1};
    ShapeType lastShape = ShapeType::Box;
    bool isScalingMode = false;
};

class BuildingSystem {
public:
    BuildingSystem();
    ~BuildingSystem() = default;

    void Update(float deltaTime, const glm::vec2& mousePosScreen, const glm::vec2& screenResolution);
    void HandleClick(bool isLeftDown, bool isRightDown, const glm::vec3& cameraRayOrigin, const glm::vec3& cameraRayDir);
    
    void ToggleBuildMode();
    void ToggleGizmoMode() { m_gizmo.isScalingMode = !m_gizmo.isScalingMode; }
    void SetState(BuildModeState newState);
    
    void RenderUI(::ImDrawList* drawList, const glm::vec2& screenCenter, const glm::mat4& viewProjMatrix, const glm::vec2& screenRes);

    BuildModeState GetCurrentState() const { return m_currentState; }
    const RadialMenu& GetCurrentMenu() const { return m_currentMenu; }
    const PlacementGizmo& GetGizmo() const { return m_gizmo; }
    void ClearPreviewDirty() { m_gizmo.previewDirty = false; }

private:
    void UpdateRadialMenu(const glm::vec2& mousePosScreen, const glm::vec2& screenResolution);
    void UpdateGizmoInteraction(const glm::vec3& cameraRayOrigin, const glm::vec3& cameraRayDir, bool isMouseDown);
    void UpdatePreviewVoxels();

    bool RaycastGizmoAxis(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& arrowOrigin, const glm::vec3& axisDir, glm::vec3& outHitPoint);
    bool RaycastGizmoRing(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float radius, glm::vec3& outHitPoint);

    void PlaceShape(int material);
    void GenerateBox(const glm::ivec3& center, const glm::ivec3& extents, int material);
    void GenerateSphere(const glm::ivec3& center, const glm::ivec3& extents, int material);
    void GenerateCylinder(const glm::ivec3& center, const glm::ivec3& extents, int material);
    void GeneratePyramid(const glm::ivec3& center, const glm::ivec3& extents, int material);

    BuildModeState m_currentState = BuildModeState::Inactive;
    RadialMenu m_mainMenu;
    RadialMenu m_shapesMenu;
    RadialMenu m_currentMenu;
    PlacementGizmo m_gizmo;
    
    bool m_wasLeftDown = false;
    bool m_wasRightDown = false;
};

} // namespace UI
