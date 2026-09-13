#include "BuildingSystem.h"
#include "../world/World.h"
#include "../core/Globals.h"
#include <cmath>
#include <iostream>
#include <imgui.h>

#define PI 3.14159265359f

namespace UI {

BuildingSystem::BuildingSystem() {
    // Initialize Main Menu
    m_mainMenu.items.push_back({"Shapes", 0, ShapeType::Box});
    m_mainMenu.items.push_back({"Prefabs", 1, ShapeType::Box});

    // Initialize Shapes Menu
    m_shapesMenu.items.push_back({"Box", 0, ShapeType::Box});
    m_shapesMenu.items.push_back({"Sphere", 1, ShapeType::Sphere});
    m_shapesMenu.items.push_back({"Cylinder", 2, ShapeType::Cylinder});
    m_shapesMenu.items.push_back({"Pyramid", 3, ShapeType::Pyramid});
    m_shapesMenu.items.push_back({"Arrow", 4, ShapeType::Arrow});
    m_shapesMenu.items.push_back({"Hexagon", 5, ShapeType::Hexagon});
    m_shapesMenu.items.push_back({"Bar", 6, ShapeType::Bar});
    
    m_currentMenu = m_mainMenu;
}

void BuildingSystem::ToggleBuildMode() {
    if (m_currentState == BuildModeState::Inactive) {
        SetState(BuildModeState::RadialMenu_Main);
    } else {
        SetState(BuildModeState::Inactive);
    }
}

void BuildingSystem::SetState(BuildModeState newState) {
    m_currentState = newState;
    if (newState == BuildModeState::RadialMenu_Main) {
        m_currentMenu = m_mainMenu;
    } else if (newState == BuildModeState::RadialMenu_Shapes) {
        m_currentMenu = m_shapesMenu;
    }
}

void BuildingSystem::Update(float deltaTime, const glm::vec2& mousePosScreen, const glm::vec2& screenResolution) {
    if (m_currentState == BuildModeState::RadialMenu_Main || m_currentState == BuildModeState::RadialMenu_Shapes) {
        UpdateRadialMenu(mousePosScreen, screenResolution);
    }
    UpdatePreviewVoxels();
}

void BuildingSystem::UpdateRadialMenu(const glm::vec2& mousePosScreen, const glm::vec2& screenResolution) {
    glm::vec2 screenCenter = screenResolution * 0.5f;
    glm::vec2 dir = mousePosScreen - screenCenter;
    
    // Calculate angle in radians
    float angle = std::atan2(dir.y, dir.x);
    if (angle < 0.0f) {
        angle += 2.0f * PI;
    }
    
    m_currentMenu.currentPointerAngle = angle;
    
    // Determine hovered index based on angle slices
    if (m_currentMenu.items.empty()) return;
    
    float sliceAngle = (2.0f * PI) / m_currentMenu.items.size();
    
    // Shift by half a slice so the slice is centered on its angle
    float shiftedAngle = angle + (sliceAngle * 0.5f);
    if (shiftedAngle >= 2.0f * PI) {
        shiftedAngle -= 2.0f * PI;
    }
    
    m_currentMenu.hoveredIndex = static_cast<int>(shiftedAngle / sliceAngle);
    if (m_currentMenu.hoveredIndex >= static_cast<int>(m_currentMenu.items.size())) {
        m_currentMenu.hoveredIndex = 0;
    }
    if (m_currentMenu.hoveredIndex < 0) {
        m_currentMenu.hoveredIndex = 0;
    }
}

void BuildingSystem::HandleClick(bool isLeftDown, bool isRightDown, const glm::vec3& cameraRayOrigin, const glm::vec3& cameraRayDir) {
    bool leftClicked = (isLeftDown && !m_wasLeftDown);
    bool rightClicked = (isRightDown && !m_wasRightDown);
    bool leftReleased = (!isLeftDown && m_wasLeftDown);
    
    if (m_currentState == BuildModeState::RadialMenu_Main || m_currentState == BuildModeState::RadialMenu_Shapes) {
        if (leftClicked && m_currentMenu.hoveredIndex != -1) {
            const auto& selectedItem = m_currentMenu.items[m_currentMenu.hoveredIndex];
            
            if (m_currentState == BuildModeState::RadialMenu_Main) {
                if (selectedItem.label == "Shapes") {
                    SetState(BuildModeState::RadialMenu_Shapes);
                }
            } else if (m_currentState == BuildModeState::RadialMenu_Shapes) {
                m_gizmo.selectedShape = selectedItem.shapeType;
                m_gizmo.extents = glm::vec3(5.0f * voxelSize); // Start at 10x10x10 size
                m_gizmo.previewDirty = true;
                SetState(BuildModeState::GizmoPreview);
            }
            m_wasLeftDown = true; // Prevent falling through to the next state this frame
            return;
        }
    } else if (m_currentState == BuildModeState::GizmoPreview) {
        // Find placement point on ground using a grid raycast
        float step = voxelSize * 0.4f;
        glm::ivec3 lastEmptyVox(-1);
        glm::ivec3 hitVox(-1);
        bool hit = false;
        
        for (float d = 0.0f; d <= 20.0f; d += step) {
            glm::vec3 pt = cameraRayOrigin + cameraRayDir * d;
            glm::ivec3 currVox(static_cast<int>(floor(pt.x / voxelSize)), 
                               static_cast<int>(floor(pt.y / voxelSize)), 
                               static_cast<int>(floor(pt.z / voxelSize)));
                               
            if (currVox.x >= 0 && currVox.x < GRID_SIZE && currVox.y >= 0 && currVox.y < GRID_SIZE && currVox.z >= 0 && currVox.z < GRID_SIZE) {
                if (getVoxel(currVox.x, currVox.y, currVox.z) > 0) {
                    hitVox = currVox;
                    hit = true;
                    break;
                } else {
                    lastEmptyVox = currVox;
                }
            }
        }
        
        if (hit && lastEmptyVox.x != -1) {
            // Find normal
            glm::ivec3 normal = lastEmptyVox - hitVox;
            // Clamp normal just in case of diagonal step
            if (std::abs(normal.x) > 0) { normal.y = 0; normal.z = 0; }
            else if (std::abs(normal.y) > 0) { normal.z = 0; }
            
            // Offset the center so the edge touches the hit surface
            glm::vec3 extentsVox = glm::floor(m_gizmo.extents / voxelSize);
            glm::vec3 centerVox = glm::vec3(hitVox) + glm::vec3(normal) * (1.0f + extentsVox);
            
            m_gizmo.position = centerVox * voxelSize;
        } else {
            // Nothing hit, place it exactly 10 units away, but snap to grid
            glm::vec3 pt = cameraRayOrigin + cameraRayDir * 10.0f;
            glm::ivec3 currVox(static_cast<int>(floor(pt.x / voxelSize)), 
                               static_cast<int>(floor(pt.y / voxelSize)), 
                               static_cast<int>(floor(pt.z / voxelSize)));
            m_gizmo.position = glm::vec3(currVox) * voxelSize;
        }
        
        // Clamp to not go below ground 0
        float minY = m_gizmo.extents.y;
        if (m_gizmo.position.y < minY) m_gizmo.position.y = minY;

        if (rightClicked) {
            SetState(BuildModeState::GizmoAnchored);
        }
    } else if (m_currentState == BuildModeState::GizmoAnchored) {
        UpdateGizmoInteraction(cameraRayOrigin, cameraRayDir, isLeftDown);
        
        if (leftReleased && m_gizmo.activeDrag == GizmoDragState::None) {
            // Confirm placement if we just clicked (not dragging)
            PlaceShape(currentBuildMaterial);
            SetState(BuildModeState::Inactive);
        }
        
        if (leftReleased) {
            m_gizmo.activeDrag = GizmoDragState::None;
        }
    }
    
    m_wasLeftDown = isLeftDown;
    m_wasRightDown = isRightDown;
}

void BuildingSystem::UpdateGizmoInteraction(const glm::vec3& cameraRayOrigin, const glm::vec3& cameraRayDir, bool isLeftDown) {
    bool leftClicked = (isLeftDown && !m_wasLeftDown);
    
    if (leftClicked) {
        glm::vec3 hitPoint;
        
        if (!m_gizmo.isScalingMode) {
            // Check Move Arrows (at center)
            if (RaycastGizmoAxis(cameraRayOrigin, cameraRayDir, m_gizmo.position, glm::vec3(1,0,0), hitPoint)) {
                m_gizmo.activeDrag = GizmoDragState::MoveX;
            } else if (RaycastGizmoAxis(cameraRayOrigin, cameraRayDir, m_gizmo.position, glm::vec3(0,1,0), hitPoint)) {
                m_gizmo.activeDrag = GizmoDragState::MoveY;
            } else if (RaycastGizmoAxis(cameraRayOrigin, cameraRayDir, m_gizmo.position, glm::vec3(0,0,1), hitPoint)) {
                m_gizmo.activeDrag = GizmoDragState::MoveZ;
            }
        } else {
            // Check Scale Arrows (at faces)
            if (RaycastGizmoAxis(cameraRayOrigin, cameraRayDir, m_gizmo.position + glm::vec3(m_gizmo.extents.x, 0, 0), glm::vec3(1,0,0), hitPoint)) {
                m_gizmo.activeDrag = GizmoDragState::ScalePX;
            } else if (RaycastGizmoAxis(cameraRayOrigin, cameraRayDir, m_gizmo.position - glm::vec3(m_gizmo.extents.x, 0, 0), glm::vec3(-1,0,0), hitPoint)) {
                m_gizmo.activeDrag = GizmoDragState::ScaleNX;
            } else if (RaycastGizmoAxis(cameraRayOrigin, cameraRayDir, m_gizmo.position + glm::vec3(0, m_gizmo.extents.y, 0), glm::vec3(0,1,0), hitPoint)) {
                m_gizmo.activeDrag = GizmoDragState::ScalePY;
            } else if (RaycastGizmoAxis(cameraRayOrigin, cameraRayDir, m_gizmo.position - glm::vec3(0, m_gizmo.extents.y, 0), glm::vec3(0,-1,0), hitPoint)) {
                m_gizmo.activeDrag = GizmoDragState::ScaleNY;
            } else if (RaycastGizmoAxis(cameraRayOrigin, cameraRayDir, m_gizmo.position + glm::vec3(0, 0, m_gizmo.extents.z), glm::vec3(0,0,1), hitPoint)) {
                m_gizmo.activeDrag = GizmoDragState::ScalePZ;
            } else if (RaycastGizmoAxis(cameraRayOrigin, cameraRayDir, m_gizmo.position - glm::vec3(0, 0, m_gizmo.extents.z), glm::vec3(0,0,-1), hitPoint)) {
                m_gizmo.activeDrag = GizmoDragState::ScaleNZ;
            }
        }

        if (m_gizmo.activeDrag != GizmoDragState::None) {
            m_gizmo.dragStartWorldPos = hitPoint;
            m_gizmo.dragStartGizmoPos = m_gizmo.position;
            m_gizmo.dragStartExtents = m_gizmo.extents;
        }
    } else if (isLeftDown && m_gizmo.activeDrag != GizmoDragState::None) {
        glm::vec3 hitPoint;
        
        // Move Logic
        if (m_gizmo.activeDrag == GizmoDragState::MoveX && RaycastGizmoAxis(cameraRayOrigin, cameraRayDir, m_gizmo.dragStartGizmoPos, glm::vec3(1,0,0), hitPoint)) {
            float diff = hitPoint.x - m_gizmo.dragStartWorldPos.x;
            m_gizmo.position.x = m_gizmo.dragStartGizmoPos.x + diff;
        } else if (m_gizmo.activeDrag == GizmoDragState::MoveY && RaycastGizmoAxis(cameraRayOrigin, cameraRayDir, m_gizmo.dragStartGizmoPos, glm::vec3(0,1,0), hitPoint)) {
            float diff = hitPoint.y - m_gizmo.dragStartWorldPos.y;
            m_gizmo.position.y = m_gizmo.dragStartGizmoPos.y + diff;
        } else if (m_gizmo.activeDrag == GizmoDragState::MoveZ && RaycastGizmoAxis(cameraRayOrigin, cameraRayDir, m_gizmo.dragStartGizmoPos, glm::vec3(0,0,1), hitPoint)) {
            float diff = hitPoint.z - m_gizmo.dragStartWorldPos.z;
            m_gizmo.position.z = m_gizmo.dragStartGizmoPos.z + diff;
        }
        
        // Scale Logic (extents + position shift so opposite face stays anchored)
        auto handleScale = [&](GizmoDragState state, const glm::vec3& axis, float& posComp, float& extComp, float startPosComp, float startExtComp, float startHitComp) {
            if (m_gizmo.activeDrag == state && RaycastGizmoAxis(cameraRayOrigin, cameraRayDir, m_gizmo.dragStartGizmoPos + axis * startExtComp, axis, hitPoint)) {
                float currentHitComp = (axis.x != 0) ? hitPoint.x : ((axis.y != 0) ? hitPoint.y : hitPoint.z);
                float diff = (currentHitComp - startHitComp) * ((axis.x + axis.y + axis.z) > 0 ? 1.0f : -1.0f);
                
                float newExt = std::fmax(voxelSize, startExtComp + diff * 0.5f);
                extComp = newExt;
                posComp = startPosComp + (newExt - startExtComp) * ((axis.x + axis.y + axis.z) > 0 ? 1.0f : -1.0f);
            }
        };

        handleScale(GizmoDragState::ScalePX, glm::vec3(1,0,0), m_gizmo.position.x, m_gizmo.extents.x, m_gizmo.dragStartGizmoPos.x, m_gizmo.dragStartExtents.x, m_gizmo.dragStartWorldPos.x);
        handleScale(GizmoDragState::ScaleNX, glm::vec3(-1,0,0), m_gizmo.position.x, m_gizmo.extents.x, m_gizmo.dragStartGizmoPos.x, m_gizmo.dragStartExtents.x, m_gizmo.dragStartWorldPos.x);
        handleScale(GizmoDragState::ScalePY, glm::vec3(0,1,0), m_gizmo.position.y, m_gizmo.extents.y, m_gizmo.dragStartGizmoPos.y, m_gizmo.dragStartExtents.y, m_gizmo.dragStartWorldPos.y);
        handleScale(GizmoDragState::ScaleNY, glm::vec3(0,-1,0), m_gizmo.position.y, m_gizmo.extents.y, m_gizmo.dragStartGizmoPos.y, m_gizmo.dragStartExtents.y, m_gizmo.dragStartWorldPos.y);
        handleScale(GizmoDragState::ScalePZ, glm::vec3(0,0,1), m_gizmo.position.z, m_gizmo.extents.z, m_gizmo.dragStartGizmoPos.z, m_gizmo.dragStartExtents.z, m_gizmo.dragStartWorldPos.z);
        handleScale(GizmoDragState::ScaleNZ, glm::vec3(0,0,-1), m_gizmo.position.z, m_gizmo.extents.z, m_gizmo.dragStartGizmoPos.z, m_gizmo.dragStartExtents.z, m_gizmo.dragStartWorldPos.z);
    }
}

// Basic placeholder math for raycasting
// Line-to-line intersection for picking and dragging arrows
bool BuildingSystem::RaycastGizmoAxis(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& arrowOrigin, const glm::vec3& axisDir, glm::vec3& outHitPoint) {
    glm::vec3 w0 = arrowOrigin - rayOrigin;
    float a = glm::dot(axisDir, axisDir);
    float b = glm::dot(axisDir, rayDir);
    float c = glm::dot(rayDir, rayDir);
    float d = glm::dot(axisDir, w0);
    float e = glm::dot(rayDir, w0);

    float denom = a * c - b * b;
    if (denom < 1e-6f) return false;

    float s = (b * e - c * d) / denom; // parameter for axis
    float t = (a * e - b * d) / denom; // parameter for ray

    if (t < 0.0f) return false; // Behind camera

    glm::vec3 pOnAxis = arrowOrigin + s * axisDir;
    glm::vec3 pOnRay = rayOrigin + t * rayDir;

    float dist = glm::distance(pOnAxis, pOnRay);
    float hitRadius = std::fmax(voxelSize * 10.0f, t * 0.02f); // Hit threshold scales with distance

    // If currently dragging THIS axis, we always return true to allow unconstrained movement
    bool isDraggingThisAxis = false;
    if (m_gizmo.activeDrag != GizmoDragState::None) isDraggingThisAxis = true; // Simplification: if dragging anything, let it return true for its raycast check

    if (dist < hitRadius || isDraggingThisAxis) {
        // Also check if they clicked on the positive side of the axis (the arrow shaft)
        if (s > 0.0f || isDraggingThisAxis) {
            outHitPoint = pOnAxis;
            return true;
        }
    }

    return false;
}

bool BuildingSystem::RaycastGizmoRing(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float radius, glm::vec3& outHitPoint) {
    // Intersect with XZ plane at gizmo's height
    float t = (m_gizmo.position.y - rayOrigin.y) / rayDir.y;
    if (t > 0.0f) {
        outHitPoint = rayOrigin + rayDir * t;
        float distToCenter = glm::distance(glm::vec3(outHitPoint.x, 0, outHitPoint.z), glm::vec3(m_gizmo.position.x, 0, m_gizmo.position.z));
        
        // Return true if clicked near the ring or plane (depending on radius check)
        if (radius == 0.0f) return true; // Just checking plane
        
        if (std::abs(distToCenter - radius) < 1.0f) { // Tolerance of 1 unit
            return true;
        }
    }
    return false;
}

void BuildingSystem::RenderUI(::ImDrawList* drawList, const glm::vec2& screenCenter, const glm::mat4& viewProjMatrix, const glm::vec2& screenRes) {
    if (m_currentState == BuildModeState::RadialMenu_Main || m_currentState == BuildModeState::RadialMenu_Shapes) {
        // Draw Radial Menu
        drawList->AddCircleFilled(ImVec2(screenCenter.x, screenCenter.y), m_currentMenu.radius, IM_COL32(30, 30, 30, 200));
        drawList->AddCircle(ImVec2(screenCenter.x, screenCenter.y), m_currentMenu.radius, IM_COL32(200, 200, 200, 255), 32, 2.0f);
        
        // Draw Pointer Arrow
        float arrowLen = m_currentMenu.radius * 0.8f;
        ImVec2 pCenter(screenCenter.x, screenCenter.y);
        ImVec2 pTip(screenCenter.x + std::cos(m_currentMenu.currentPointerAngle) * arrowLen,
                    screenCenter.y + std::sin(m_currentMenu.currentPointerAngle) * arrowLen);
        ImVec2 pLeft(screenCenter.x + std::cos(m_currentMenu.currentPointerAngle - 0.2f) * (arrowLen * 0.8f),
                     screenCenter.y + std::sin(m_currentMenu.currentPointerAngle - 0.2f) * (arrowLen * 0.8f));
        ImVec2 pRight(screenCenter.x + std::cos(m_currentMenu.currentPointerAngle + 0.2f) * (arrowLen * 0.8f),
                      screenCenter.y + std::sin(m_currentMenu.currentPointerAngle + 0.2f) * (arrowLen * 0.8f));
        
        drawList->AddTriangleFilled(pTip, pLeft, pRight, IM_COL32(255, 200, 50, 255));
        
        // Draw Items
        if (!m_currentMenu.items.empty()) {
            float sliceAngle = (2.0f * PI) / m_currentMenu.items.size();
            for (size_t i = 0; i < m_currentMenu.items.size(); ++i) {
                float itemAngle = i * sliceAngle;
                float labelDist = m_currentMenu.radius * 0.6f;
                ImVec2 labelPos(screenCenter.x + std::cos(itemAngle) * labelDist,
                                screenCenter.y + std::sin(itemAngle) * labelDist);
                
                ImU32 col = (i == m_currentMenu.hoveredIndex) ? IM_COL32(255, 255, 255, 255) : IM_COL32(150, 150, 150, 255);
                drawList->AddText(ImVec2(labelPos.x - 20, labelPos.y - 10), col, m_currentMenu.items[i].label.c_str());
            }
        }
    }
}

void BuildingSystem::PlaceShape(int material) {
    // Convert world extents back into voxel extents
    glm::ivec3 extents(
        std::max(0, static_cast<int>(std::round(m_gizmo.extents.x / voxelSize - 0.5f))),
        std::max(0, static_cast<int>(std::round(m_gizmo.extents.y / voxelSize - 0.5f))),
        std::max(0, static_cast<int>(std::round(m_gizmo.extents.z / voxelSize - 0.5f)))
    );
    glm::ivec3 center = glm::ivec3(glm::round(m_gizmo.position / voxelSize));

    switch (m_gizmo.selectedShape) {
        case ShapeType::Box: GenerateBox(center, extents, material); break;
        case ShapeType::Sphere: GenerateSphere(center, extents, material); break;
        case ShapeType::Cylinder: GenerateCylinder(center, extents, material); break;
        case ShapeType::Pyramid: GeneratePyramid(center, extents, material); break;
        default: GenerateBox(center, extents, material); break;
    }
    
    updateStaticMesh(glm::vec3(0,0,0), 1000.0f);
}

void BuildingSystem::GenerateBox(const glm::ivec3& center, const glm::ivec3& extents, int material) {
    for (int x = center.x - extents.x; x <= center.x + extents.x; ++x) {
        for (int y = center.y - extents.y; y <= center.y + extents.y; ++y) {
            for (int z = center.z - extents.z; z <= center.z + extents.z; ++z) {
                if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE && z >= 0 && z < GRID_SIZE) {
                    setVoxel(x, y, z, material);
                    markChunkDirty(x, y, z);
                }
            }
        }
    }
}

void BuildingSystem::GenerateSphere(const glm::ivec3& center, const glm::ivec3& extents, int material) {
    for (int x = center.x - extents.x; x <= center.x + extents.x; ++x) {
        for (int y = center.y - extents.y; y <= center.y + extents.y; ++y) {
            for (int z = center.z - extents.z; z <= center.z + extents.z; ++z) {
                float dx = (x - center.x) / static_cast<float>(extents.x);
                float dy = (y - center.y) / static_cast<float>(extents.y);
                float dz = (z - center.z) / static_cast<float>(extents.z);
                
                if (dx*dx + dy*dy + dz*dz <= 1.0f) {
                    if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE && z >= 0 && z < GRID_SIZE) {
                        setVoxel(x, y, z, material);
                        markChunkDirty(x, y, z);
                    }
                }
            }
        }
    }
}

void BuildingSystem::GenerateCylinder(const glm::ivec3& center, const glm::ivec3& extents, int material) {
    for (int x = center.x - extents.x; x <= center.x + extents.x; ++x) {
        for (int y = center.y - extents.y; y <= center.y + extents.y; ++y) {
            for (int z = center.z - extents.z; z <= center.z + extents.z; ++z) {
                float dx = (x - center.x) / static_cast<float>(extents.x);
                float dz = (z - center.z) / static_cast<float>(extents.z);
                
                if (dx*dx + dz*dz <= 1.0f) {
                    if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE && z >= 0 && z < GRID_SIZE) {
                        setVoxel(x, y, z, material);
                        markChunkDirty(x, y, z);
                    }
                }
            }
        }
    }
}

void BuildingSystem::GeneratePyramid(const glm::ivec3& center, const glm::ivec3& extents, int material) {
    for (int y = center.y - extents.y; y <= center.y + extents.y; ++y) {
        float normalizedY = (y - (center.y - extents.y)) / static_cast<float>(2 * extents.y);
        float widthAtY = 1.0f - normalizedY;
        int curExtentsX = static_cast<int>(extents.x * widthAtY);
        int curExtentsZ = static_cast<int>(extents.z * widthAtY);
        
        for (int x = center.x - curExtentsX; x <= center.x + curExtentsX; ++x) {
            for (int z = center.z - curExtentsZ; z <= center.z + curExtentsZ; ++z) {
                if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE && z >= 0 && z < GRID_SIZE) {
                    setVoxel(x, y, z, material);
                    markChunkDirty(x, y, z);
                }
            }
        }
    }
}

void BuildingSystem::UpdatePreviewVoxels() {
    glm::ivec3 extents(
        std::max(0, static_cast<int>(std::round(m_gizmo.extents.x / voxelSize - 0.5f))),
        std::max(0, static_cast<int>(std::round(m_gizmo.extents.y / voxelSize - 0.5f))),
        std::max(0, static_cast<int>(std::round(m_gizmo.extents.z / voxelSize - 0.5f)))
    );
    
    // Prevent massive allocations in preview (max 50 voxels half-extent)
    extents.x = std::min(extents.x, 50);
    extents.y = std::min(extents.y, 50);
    extents.z = std::min(extents.z, 50);

    // Only rebuild if the voxel dimensions or shape actually changed
    if (!m_gizmo.previewDirty && m_gizmo.lastIntExtents == extents && m_gizmo.lastShape == m_gizmo.selectedShape) {
        return;
    }
    
    m_gizmo.previewDirty = true; // Tell renderer to rebuild VAO
    m_gizmo.lastIntExtents = extents;
    m_gizmo.lastShape = m_gizmo.selectedShape;
    
    m_gizmo.previewVoxels.clear();

    for (int x = -extents.x; x <= extents.x; ++x) {
        for (int y = -extents.y; y <= extents.y; ++y) {
            for (int z = -extents.z; z <= extents.z; ++z) {
                bool inside = false;
                
                if (m_gizmo.selectedShape == ShapeType::Box) {
                    inside = true;
                } else if (m_gizmo.selectedShape == ShapeType::Sphere) {
                    float dx = x / static_cast<float>(extents.x);
                    float dy = y / static_cast<float>(extents.y);
                    float dz = z / static_cast<float>(extents.z);
                    if (dx*dx + dy*dy + dz*dz <= 1.0f) inside = true;
                } else if (m_gizmo.selectedShape == ShapeType::Cylinder) {
                    float dx = x / static_cast<float>(extents.x);
                    float dz = z / static_cast<float>(extents.z);
                    if (dx*dx + dz*dz <= 1.0f) inside = true;
                } else if (m_gizmo.selectedShape == ShapeType::Pyramid) {
                    float maxDist = std::fmax(std::abs(x) / static_cast<float>(extents.x), std::abs(z) / static_cast<float>(extents.z));
                    float heightFactor = 1.0f - ((y + extents.y) / (2.0f * extents.y));
                    if (maxDist <= heightFactor) inside = true;
                }
                
                if (inside) {
                    m_gizmo.previewVoxels.push_back(glm::vec3(x, y, z) * voxelSize);
                }
            }
        }
    }

}

} // namespace UI
