#include "stdafx.h"

#include "Headers/SolutionExplorerWindow.h"

#include "Editor/Headers/Editor.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/PlatformContext.h"
#include "Managers/Headers/SceneManager.h"
#include "Rendering/Camera/Headers/Camera.h"
#include "Geometry/Shapes/Headers/Object3D.h"
#include "Dynamics/Entities/Units/Headers/Player.h"

#include "Dynamics/Entities/Particles/ConcreteGenerators/Headers/ParticleBoxGenerator.h"
#include "Dynamics/Entities/Particles/ConcreteGenerators/Headers/ParticleColourGenerator.h"
#include "Dynamics/Entities/Particles/ConcreteGenerators/Headers/ParticleVelocityGenerator.h"
#include "Dynamics/Entities/Particles/ConcreteGenerators/Headers/ParticleTimeGenerator.h"
#include "Dynamics/Entities/Particles/ConcreteUpdaters/Headers/ParticleBasicTimeUpdater.h"
#include "Dynamics/Entities/Particles/ConcreteUpdaters/Headers/ParticleBasicColourUpdater.h"
#include "Dynamics/Entities/Particles/ConcreteUpdaters/Headers/ParticleEulerUpdater.h"
#include "Dynamics/Entities/Particles/ConcreteUpdaters/Headers/ParticleFloorUpdater.h"

#include <imgui/imgui_internal.h>
#include <EASTL/deque.h>

namespace Divide {
    namespace {
        constexpr U8 g_maxEntryCount = 32;
        eastl::deque<F32> g_framerateBuffer;
        vectorEASTL<F32> g_framerateBufferCont;
        SceneNodeType g_currentNodeType = SceneNodeType::TYPE_TRANSFORM;
        SceneGraphNodeDescriptor g_nodeDescriptor;
        std::shared_ptr<ParticleData> g_particleEmitterData = nullptr;
        std::shared_ptr<ParticleSource> g_particleSource = nullptr;
        F32 g_particleBounceFactor = 0.65f;
        vec3<F32> g_particleAcceleration = {0.f, -20.f, 0.f};
        FColour4 g_particleStartColour = DefaultColours::BLACK;
        FColour4 g_particleEndColour = DefaultColours::WHITE;
    };

    SolutionExplorerWindow::SolutionExplorerWindow(Editor& parent, PlatformContext& context, const Descriptor& descriptor)
        : DockedWindow(parent, descriptor),
          PlatformContextComponent(context)
    {
        g_framerateBufferCont.reserve(g_maxEntryCount);
    }

    SolutionExplorerWindow::~SolutionExplorerWindow()
    {

    }

    void SolutionExplorerWindow::printCameraNode(SceneManager& sceneManager, Camera* camera) {
        if (camera == nullptr) {
            return;
        }

        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Leaf;
        if (_filter.PassFilter(camera->resourceName().c_str())) {
            if (ImGui::TreeNodeEx((void*)(intptr_t)camera->getGUID(), node_flags, camera->resourceName().c_str())) {
                if (ImGui::IsItemClicked()) {
                    sceneManager.resetSelection(0);
                    if (Attorney::EditorSolutionExplorerWindow::getSelectedCamera(_parent) == camera) {
                        Attorney::EditorSolutionExplorerWindow::setSelectedCamera(_parent, nullptr);
                    } else {
                        Attorney::EditorSolutionExplorerWindow::setSelectedCamera(_parent, camera);
                    }
                }

                ImGui::TreePop();
            }
        }
    }

    void SolutionExplorerWindow::drawContextMenu(SceneGraphNode& sgn) {
        if (ImGui::BeginPopupContextItem("Context menu")) {
            const SceneNode& node = sgn.getNode();
            const bool isSubMesh = node.type() == SceneNodeType::TYPE_OBJECT3D && static_cast<const Object3D&>(node).getObjectType()._value == ObjectType::SUBMESH;
            const bool isRoot = sgn.parent() == nullptr;

            static F32 value = 0.0f;
            ImGui::Text(sgn.name().c_str());
            ImGui::Separator();
            if (isSubMesh) {
                PushReadOnly();
            }
            if (ImGui::Selectable("Change Parent")) {
                _childNode = &sgn;
                _reparentSelectRequested = true;
            }
            if (isSubMesh) {
                PopReadOnly();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Can't re-parent sub-meshes!");
                }
            }
            if (ImGui::Selectable("Add Child")) {
                g_particleEmitterData.reset();
                g_particleSource.reset();

                g_nodeDescriptor = {};
                g_nodeDescriptor._name = Util::StringFormat("New_Child_Node_%d", sgn.getGUID());
                g_nodeDescriptor._componentMask = to_U32(ComponentType::TRANSFORM) | to_U32(ComponentType::BOUNDS);
                g_currentNodeType = SceneNodeType::TYPE_TRANSFORM;
                _parentNode = &sgn;
            }
            ImGui::Separator();
            if (isRoot) {
                PushReadOnly();
            }
            if (ImGui::Selectable("Remove")) {
                _nodeToRemove = sgn.getGUID();
            }
            if (isRoot) {
                PopReadOnly();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Can't remove the root node!");
                }
            }
            ImGui::Separator();

            if (ImGui::Selectable("Go To")) {
                goToNode(sgn);
            }
            if (ImGui::Selectable("Save Changes")) {
                saveNode(sgn);
            }
            if (ImGui::Selectable("Load from file")) {
                loadNode(sgn);
            }
            ImGui::EndPopup();
        }
    }

    void SolutionExplorerWindow::printSceneGraphNode(SceneManager& sceneManager, SceneGraphNode& sgn, I32 nodeIDX, bool open, bool secondaryView) {
        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow;
                                        //Conflicts with "Teleport to node on double click"
                                        // | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        const bool wasSelected = secondaryView ? (_tempParent != nullptr && _tempParent->getGUID() == sgn.getGUID()) : sgn.hasFlag(SceneGraphNode::Flags::SELECTED);

        if (open) {
            node_flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        if (wasSelected) {
            node_flags |= ImGuiTreeNodeFlags_Selected;
        }

        if (sgn.getChildCount() == 0u) {
            node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
        }

        const auto printNode = [&]() {
            const bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)sgn.getGUID(), node_flags, Util::StringFormat("[%d] %s", nodeIDX, sgn.name().c_str()).c_str());
            const bool isRoot = sgn.parent() == nullptr;
            if (!secondaryView && wasSelected) {
                drawContextMenu(sgn);
            }

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                if (secondaryView) {
                    _tempParent = &sgn;
                } else {
                    const bool parentSelected = (!isRoot && sgn.parent()->hasFlag(SceneGraphNode::Flags::SELECTED));
                    const bool childrenSelected = sgn.getChildCount() > 0 && sgn.getChild(0u).hasFlag(SceneGraphNode::Flags::SELECTED);

                    sceneManager.resetSelection(0);
                    if (!wasSelected || parentSelected || childrenSelected) {
                        sceneManager.setSelected(0, { &sgn }, !wasSelected);
                    }
                    Attorney::EditorSolutionExplorerWindow::setSelectedCamera(_parent, nullptr);
                }
            }
            if (!secondaryView && ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered(ImGuiHoveredFlags_None)) {
                goToNode(sgn);
            }
            return nodeOpen;
        };

        if (_filter.Filters.empty()) {
            if (printNode()) {
                sgn.forEachChild([this, &sceneManager, secondaryView](SceneGraphNode* child, I32 childIdx) {
                    printSceneGraphNode(sceneManager, *child, childIdx, false, secondaryView);
                    return true;
                });
                ImGui::TreePop();
            }
        } else {
            bool nodeOpen = false;
            if (_filter.PassFilter(sgn.name().c_str())) {
                nodeOpen = printNode();
            }
            sgn.forEachChild([this, &sceneManager, secondaryView](SceneGraphNode* child, I32 childIdx) {
                printSceneGraphNode(sceneManager, *child, childIdx, false, secondaryView);
                return true;
            });
            if (nodeOpen) {
                ImGui::TreePop();
            }
        }
    }

    void SolutionExplorerWindow::drawInternal() {
        SceneManager& sceneManager = *context().kernel().sceneManager();
        Scene& activeScene = sceneManager.getActiveScene();
        SceneGraphNode& root = activeScene.sceneGraph().getRoot();

        ImGui::PushItemWidth(200);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Find node: ");
        ImGui::SameLine();
        ImGui::PushID("GraphSearchFilter");
        _filter.Draw("", 200);
        ImGui::PopID();

        ImGui::BeginChild("SceneGraph", ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetWindowHeight() * .5f), true, 0);
        if (ImGui::TreeNodeEx(activeScene.resourceName().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 3); // Increase spacing to differentiate leaves from expanded contents.
            for (PlayerIndex i = 0; i < static_cast<PlayerIndex>(Config::MAX_LOCAL_PLAYER_COUNT); ++i) {
                printCameraNode(sceneManager, Attorney::SceneManagerCameraAccessor::playerCamera(sceneManager, i));
            }
            printSceneGraphNode(sceneManager, root, 0, true, false);
            ImGui::PopStyleVar();
            ImGui::TreePop();
        }

        ImGui::Separator();
        ImGui::NewLine();
        ImGui::NewLine();
        ImGui::Text("All scenes");
        ImGui::Separator();

        const vectorEASTL<Str128>& scenes = sceneManager.sceneNameList();
        for (const Str128& scene : scenes) {
            if (scene != activeScene.resourceName()) {
                if (ImGui::TreeNodeEx(scene.c_str(), ImGuiTreeNodeFlags_Leaf)) {
                    ImGui::TreePop();
                }
            }
        }

        ImGui::EndChild();
        ImGui::PopItemWidth();
        ImGui::Separator();

        drawTransformSettings();

        ImGui::Separator();

        // Calculate and show framerate
        static F32 max_ms_per_frame = 0;

        static F32 ms_per_frame[g_maxEntryCount] = { 0 };
        static I32 ms_per_frame_idx = 0;
        static F32 ms_per_frame_accum = 0.0f;
        ms_per_frame_accum -= ms_per_frame[ms_per_frame_idx];
        ms_per_frame[ms_per_frame_idx] = ImGui::GetIO().DeltaTime * 1000.0f;
        ms_per_frame_accum += ms_per_frame[ms_per_frame_idx];
        ms_per_frame_idx = (ms_per_frame_idx + 1) % g_maxEntryCount;
        const F32 ms_per_frame_avg = ms_per_frame_accum / g_maxEntryCount;
        if (ms_per_frame_avg + (Config::TARGET_FRAME_RATE / 1000.0f) > max_ms_per_frame) {
            max_ms_per_frame = ms_per_frame_avg + (Config::TARGET_FRAME_RATE / 1000.0f);
        }

        // We need this bit to get a nice "flowing" feel
        g_framerateBuffer.push_back(ms_per_frame_avg);
        if (g_framerateBuffer.size() > g_maxEntryCount) {
            g_framerateBuffer.pop_front();
        }
        g_framerateBufferCont.resize(0);
        g_framerateBufferCont.insert(eastl::cbegin(g_framerateBufferCont),
                                     eastl::cbegin(g_framerateBuffer),
                                     eastl::cend(g_framerateBuffer));
        ImGui::PlotHistogram("",
                             g_framerateBufferCont.data(),
                             to_I32(g_framerateBufferCont.size()),
                             0,
                             Util::StringFormat("%.3f ms/frame (%.1f FPS)", ms_per_frame_avg, ms_per_frame_avg > 0.01f ? 1000.0f / ms_per_frame_avg : 0.0f).c_str(),
                             0.0f,
                             max_ms_per_frame,
                             ImVec2(0, 50));

        ImGui::Separator();

        static U32 cullCount = 0;
        static U32 updateCount = 3;
        static U32 crtUpdate = 0;
        if (crtUpdate++ == updateCount) {
            cullCount = context().gfx().getLastCullCount();
            crtUpdate = 0;
        }

        ImGui::Text("HiZ Cull Count: %d", cullCount);
        ImGui::Separator();

        bool dayNightEnabled = activeScene.dayNightCycleEnabled();
        if (ImGui::Checkbox("Enable day/night cycle", &dayNightEnabled)) {
            activeScene.dayNightCycleEnabled(dayNightEnabled);
        }

        ImGui::Text("Time of Day:");
        SimpleTime time = activeScene.getTimeOfDay();
        constexpr U8 min = 0u;
        constexpr U8 maxHour = 24u;
        constexpr U8 maxMinute = 59u;

        if (ImGui::SliderScalar("Hour", ImGuiDataType_U8, &time._hour, &min, &maxHour, "%02d") ||
            ImGui::SliderScalar("Minute", ImGuiDataType_U8, &time._minutes, &min, &maxMinute, "%02d"))
        {
            activeScene.setTimeOfDay(time);
        }

        if (!dayNightEnabled) {
            PushReadOnly();
        }

        F32 timeFactor = activeScene.getDayNightCycleTimeFactor();
        if (ImGui::InputFloat("Time factor", &timeFactor)) {
            activeScene.setDayNightCycleTimeFactor(CLAMPED(timeFactor, -500.f, 500.f));
        }

        const SunDetails sun = activeScene.getCurrentSunDetails();
        ImGui::Text("Sunset: %02d:%02d", sun._info.sunsetTime._hour, sun._info.sunsetTime._minutes);
        ImGui::SameLine(); ImGui::Text(" | "); ImGui::SameLine();
        ImGui::Text("Sunrise: %02d:%02d", sun._info.sunriseTime._hour, sun._info.sunriseTime._minutes);
        ImGui::SameLine();  ImGui::Text(" | "); ImGui::SameLine();
        ImGui::Text("Noon: %02d:%02d", sun._info.noonTime._hour, sun._info.noonTime._minutes);

        if (!dayNightEnabled) {
            PopReadOnly();
        }

        drawRemoveNodeDialog();
        drawAddNodeDialog();
        drawChangeParentWindow();
        drawReparentNodeDialog();
    }

    void SolutionExplorerWindow::drawRemoveNodeDialog() {
        if (_nodeToRemove == -1) {
            return;
        }

        ImGui::OpenPopup("Confirm Remove");

        if (ImGui::BeginPopupModal("Confirm Remove", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want remove the selected node [ %d ]?", _nodeToRemove);
            ImGui::Separator();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                _nodeToRemove = -1;
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Yes", ImVec2(120, 0))) {

                ImGui::CloseCurrentPopup();
                Attorney::EditorSolutionExplorerWindow::queueRemoveNode(_parent, _nodeToRemove);
                _nodeToRemove = -1;
            }
            ImGui::EndPopup();
        }
    }

    void SolutionExplorerWindow::drawReparentNodeDialog() {
        if (!_reparentConfirmRequested) {
            return;
        }

        ImGui::OpenPopup("Confirm Re-parent");

        if (ImGui::BeginPopupModal("Confirm Re-parent", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want change the selected node's [ %s ] parent?", _childNode->name().c_str());
            ImGui::Text("Old Parent [ %s ] | New Parent [ %s ]", _childNode->parent()->name().c_str(), _tempParent->name().c_str());
            ImGui::Separator();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                _reparentConfirmRequested = false;
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Yes", ImVec2(120, 0))) {
                _childNode->setParent(*_tempParent, true);
                _childNode = nullptr;
                _tempParent = nullptr;

                ImGui::CloseCurrentPopup();
                _reparentConfirmRequested = false;
            }
            ImGui::EndPopup();
        }
    }

    void SolutionExplorerWindow::drawAddNodeDialog() {
        if (_parentNode == nullptr) {
            return;
        }

        ImGui::OpenPopup("Create New Node");

        if (ImGui::BeginPopupModal("Create New Node", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Creating a child node for SGN [ %d ][ %s ]?", _parentNode->getGUID(), _parentNode->name().c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
            ImGui::BeginChild("Node Properties", ImVec2(0, 400), true, 0);

            ImGui::InputText("Name", g_nodeDescriptor._name.data(), g_nodeDescriptor._name.capacity());

            const char* currentType = Names::sceneNodeType[to_base(g_currentNodeType)];
            if (ImGui::BeginCombo("Node Type", currentType, ImGuiComboFlags_PopupAlignLeft)) {
                for (U8 t = 0; t < to_U8(SceneNodeType::COUNT); ++t) {
                    const SceneNodeType type = static_cast<SceneNodeType>(t);
                    const bool isSelected = g_currentNodeType == type;
                    const bool valid = type != SceneNodeType::TYPE_SKY &&
                                       type != SceneNodeType::TYPE_VEGETATION &&
                                       type != SceneNodeType::TYPE_OBJECT3D;

                    if (ImGui::Selectable(Names::sceneNodeType[t], isSelected, valid ? 0 : ImGuiSelectableFlags_Disabled)) {
                        g_currentNodeType = type;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::BeginChild("Components", ImVec2(0, 260), true, 0);

            U32& componentMask = g_nodeDescriptor._componentMask;
            if (g_currentNodeType == SceneNodeType::TYPE_WATER || g_currentNodeType == SceneNodeType::TYPE_PARTICLE_EMITTER) {
                componentMask |= to_U32(ComponentType::NAVIGATION) | 
                                 to_U32(ComponentType::RIGID_BODY) |
                                 to_U32(ComponentType::RENDERING);
            } else if (g_currentNodeType == SceneNodeType::TYPE_INFINITEPLANE) {
                componentMask |= to_U32(ComponentType::RENDERING);
            }

            if (g_currentNodeType == SceneNodeType::TYPE_PARTICLE_EMITTER) {
                componentMask |= to_U32(ComponentType::SELECTION);
            }

            for (ComponentType::_integral i = 1; i < ComponentType::COUNT + 1; ++i) {
                const U32 componentBit = 1 << i;
                bool required = componentBit == to_U32(ComponentType::TRANSFORM) || 
                                componentBit == to_U32(ComponentType::BOUNDS);
                if (g_currentNodeType == SceneNodeType::TYPE_WATER || g_currentNodeType == SceneNodeType::TYPE_PARTICLE_EMITTER) {
                    required = required ||
                               componentBit == to_U32(ComponentType::NAVIGATION) ||
                               componentBit == to_U32(ComponentType::RIGID_BODY) ||
                               componentBit == to_U32(ComponentType::RENDERING);
                    if (g_currentNodeType == SceneNodeType::TYPE_PARTICLE_EMITTER) {
                        required = required || componentBit == to_U32(ComponentType::SELECTION);
                    }
                } else if (g_currentNodeType == SceneNodeType::TYPE_INFINITEPLANE) {
                    required = required || componentBit == to_U32(ComponentType::RENDERING);
                }

                bool invalid = componentBit == to_U32(ComponentType::INVERSE_KINEMATICS) ||
                               componentBit == to_U32(ComponentType::ANIMATION) ||
                               componentBit == to_U32(ComponentType::RAGDOLL);
                if (required || invalid) {
                    PushReadOnly();
                }

                bool componentEnabled = BitCompare(componentMask, componentBit);
                const char* compLabel = ComponentType::_from_integral(componentBit)._to_string();
                if (ImGui::Checkbox(compLabel, &componentEnabled)) {
                    SetBit(componentMask, componentBit);
                }
                if (ImGui::IsItemHovered()) {
                    if (required) {
                        ImGui::SetTooltip("Required component for current node type!");
                    } else if (invalid) {
                        ImGui::SetTooltip("Component type not (yet) supported!");
                    }
                }
                if (required || invalid) {
                    PopReadOnly();
                }
            }

            ImGui::EndChild();
            ImGui::Separator();
            bool nodeDynamic = g_nodeDescriptor._usageContext == NodeUsageContext::NODE_DYNAMIC;
            if (ImGui::Checkbox("Dynamic", &nodeDynamic)) {
                g_nodeDescriptor._usageContext = nodeDynamic ? NodeUsageContext::NODE_DYNAMIC : NodeUsageContext::NODE_STATIC;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Static or dynamic node? Affects navigation, collision detection and other systems.");
            }

            ImGui::Checkbox("Serializable", &g_nodeDescriptor._serialize);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("State is saved and loaded to and from external files?");
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();

            drawNodeParametersChildWindow();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                _parentNode = nullptr;
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Yes", ImVec2(120, 0))) {
                g_nodeDescriptor._node = createNode();
                _parentNode->addChildNode(g_nodeDescriptor);
                g_nodeDescriptor._node.reset();
                ImGui::CloseCurrentPopup();
              
                _parentNode = nullptr;
            }
            ImGui::EndPopup();
        }
    }

    void SolutionExplorerWindow::drawNodeParametersChildWindow() {
        if (g_currentNodeType == SceneNodeType::TYPE_PARTICLE_EMITTER) {
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
            ImGui::BeginChild("Type Specific Properties", ImVec2(0, 200), true, 0);
            if (g_particleEmitterData == nullptr) {
                const U32 options =
                    to_U32(ParticleDataProperties::PROPERTIES_POS) |
                    to_U32(ParticleDataProperties::PROPERTIES_VEL) |
                    to_U32(ParticleDataProperties::PROPERTIES_ACC) |
                    to_U32(ParticleDataProperties::PROPERTIES_COLOR) |
                    to_U32(ParticleDataProperties::PROPERTIES_COLOR_TRANS);
                g_particleEmitterData = std::make_shared<ParticleData>(context().gfx(), 1000, options);
                g_particleSource = std::make_shared<ParticleSource>(context().gfx(), 250.f);

                g_particleEmitterData->_textureFileName = "particle.DDS";

                std::shared_ptr<ParticleBoxGenerator> boxGenerator = std::make_shared<ParticleBoxGenerator>();
                boxGenerator->halfExtent({ 0.3f, 0.0f, 0.3f });
                g_particleSource->addGenerator(boxGenerator);

                std::shared_ptr<ParticleColourGenerator> colGenerator = std::make_shared<ParticleColourGenerator>();
                colGenerator->_minStartCol.set(g_particleStartColour);
                colGenerator->_maxStartCol.set(g_particleStartColour);
                colGenerator->_minEndCol.set(g_particleEndColour);
                colGenerator->_maxEndCol.set(g_particleEndColour);
                g_particleSource->addGenerator(colGenerator);

                std::shared_ptr<ParticleVelocityGenerator> velGenerator = std::make_shared<ParticleVelocityGenerator>();
                velGenerator->_minStartVel = { -1.0f, 0.22f, -1.0f };
                velGenerator->_maxStartVel = { 1.0f, 3.45f, 1.0f };
                g_particleSource->addGenerator(velGenerator);

                std::shared_ptr<ParticleTimeGenerator> timeGenerator = std::make_shared<ParticleTimeGenerator>();
                timeGenerator->_minTime = 8.5f;
                timeGenerator->_maxTime = 20.5f;
                g_particleSource->addGenerator(timeGenerator);
            }

            PushReadOnly();
            ImGui::InputText("Texture File Name", g_particleEmitterData->_textureFileName.data(), 128);
            PopReadOnly();

            U32 componentMask = g_particleEmitterData->optionsMask();
            U32 particleCount = g_particleEmitterData->totalCount();
            if (ImGui::InputScalar("Particle Count", ImGuiDataType_U32, &particleCount)) {
                if (particleCount == 0) {
                    particleCount = 1;
                }
                g_particleEmitterData->generateParticles(particleCount, componentMask);
            }
            
            F32 emitRate = g_particleSource->emitRate();
            if (ImGui::InputFloat("Emit rate", &emitRate)) {
                if (emitRate <= std::numeric_limits<F32>::epsilon()) {
                    emitRate = 1.0f;
                }
                g_particleSource->updateEmitRate(emitRate);
            }

            for (U8 i = 1; i < to_U8(ParticleDataProperties::COUNT) + 1; ++i) {
                const U32 componentBit = 1 << i;
                bool componentEnabled = BitCompare(componentMask, componentBit);
                const char* compLabel = Names::particleDataProperties[i - 1];
                if (ImGui::Checkbox(compLabel, &componentEnabled)) {
                    SetBit(componentMask, componentBit);
                    g_particleEmitterData->generateParticles(particleCount, componentMask);
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
    }

    void SolutionExplorerWindow::drawTransformSettings() {
         bool enableGizmo = Attorney::EditorSolutionExplorerWindow::editorEnableGizmo(_parent);
         ImGui::Checkbox("Transform Gizmo", &enableGizmo);
         Attorney::EditorSolutionExplorerWindow::editorEnableGizmo(_parent, enableGizmo);
    }

    void SolutionExplorerWindow::goToNode(const SceneGraphNode& sgn) const {
        Attorney::EditorSolutionExplorerWindow::teleportToNode(_parent, sgn);
    }

    void SolutionExplorerWindow::saveNode(const SceneGraphNode& sgn) const {
        Attorney::EditorSolutionExplorerWindow::saveNode(_parent, sgn);
    }

    void SolutionExplorerWindow::loadNode(SceneGraphNode& sgn) const {
        Attorney::EditorSolutionExplorerWindow::loadNode(_parent, sgn);
    }

    SceneNode_ptr SolutionExplorerWindow::createNode() {
        ResourceDescriptor descriptor(Util::StringFormat("%s_node", g_nodeDescriptor._name.c_str()));
        SceneNode_ptr ptr =  Attorney::EditorSolutionExplorerWindow::createNode(_parent, g_currentNodeType, descriptor);
        if (ptr) {
            if (g_currentNodeType == SceneNodeType::TYPE_PARTICLE_EMITTER) {
                ParticleEmitter* emitter = static_cast<ParticleEmitter*>(ptr.get());
                emitter->initData(g_particleEmitterData);
                emitter->addSource(g_particleSource);

                std::shared_ptr<ParticleEulerUpdater> eulerUpdater = std::make_shared<ParticleEulerUpdater>(context());
                eulerUpdater->_globalAcceleration.set(g_particleAcceleration);
                emitter->addUpdater(eulerUpdater);
                std::shared_ptr<ParticleFloorUpdater> floorUpdater = std::make_shared<ParticleFloorUpdater>(context());
                floorUpdater->_bounceFactor = g_particleBounceFactor;
                emitter->addUpdater(floorUpdater);
                emitter->addUpdater(std::make_shared<ParticleBasicTimeUpdater>(context()));
                emitter->addUpdater(std::make_shared<ParticleBasicColourUpdater>(context()));

                g_particleEmitterData.reset();
                g_particleSource.reset();
            } else if (false) {

            }
        }
        return ptr;
    }

    void SolutionExplorerWindow::drawChangeParentWindow() {
        if (_reparentSelectRequested) {
            ImGui::OpenPopup("Select New Parent");

            if (ImGui::BeginPopupModal("Select New Parent", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                SceneManager& sceneManager = *context().kernel().sceneManager();
                Scene& activeScene = sceneManager.getActiveScene();
                SceneGraphNode& root = activeScene.sceneGraph().getRoot();

                ImGui::Text("Selecting a new parent for SGN [ %d ][ %s ]?", _childNode->getGUID(), _childNode->name().c_str());

                if (ImGui::BeginChild("SceneGraph", ImVec2(0, 400), true, 0)) {
                    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 3); // Increase spacing to differentiate leaves from expanded contents.
                    printSceneGraphNode(sceneManager, root, 0, true, true);
                    ImGui::PopStyleVar();

                    ImGui::EndChild();
                }

                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    _childNode = nullptr;
                    _tempParent = nullptr;

                    ImGui::CloseCurrentPopup();
                    _reparentSelectRequested = false;
                }

                ImGui::SameLine();
                if (ImGui::Button("Done", ImVec2(120, 0))) {
                    _reparentConfirmRequested = true;

                    ImGui::CloseCurrentPopup();
                    _reparentSelectRequested = false;
                }
                ImGui::EndPopup();
            }
        }
    }
};