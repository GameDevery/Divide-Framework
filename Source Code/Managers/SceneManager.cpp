#include "stdafx.h"

#include "Headers/SceneManager.h"
#include "Headers/FrameListenerManager.h"
#include "Headers/RenderPassManager.h"

#include "AI/PathFinding/Headers/DivideRecast.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/EngineTaskPool.h"
#include "Core/Headers/Kernel.h"
#include "Core/Headers/ParamHandler.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Headers/StringHelper.h"
#include "Core/Time/Headers/ApplicationTimer.h"
#include "Core/Time/Headers/ProfileTimer.h"
#include "GUI/Headers/GUI.h"
#include "GUI/Headers/GUIButton.h"
#include "Rendering/Camera/Headers/FreeFlyCamera.h"
#include "Rendering/Headers/Renderer.h"
#include "Rendering/PostFX/Headers/PostFX.h"
#include "Scenes/Headers/ScenePool.h"
#include "Scenes/Headers/SceneShaderData.h"

#include "Environment/Vegetation/Headers/Vegetation.h"
#include "Environment/Water/Headers/Water.h"

#include "Dynamics/Entities/Units/Headers/Player.h"
#include "Geometry/Importer/Headers/DVDConverter.h"

#include "ECS/Components/Headers/BoundsComponent.h"
#include "ECS/Components/Headers/DirectionalLightComponent.h"
#include "ECS/Components/Headers/SelectionComponent.h"
#include "ECS/Components/Headers/TransformComponent.h"
#include "ECS/Components/Headers/UnitComponent.h"

namespace Divide {

bool SceneManager::onStartup() {
    if (RenderPassCuller::onStartup()) {
        return Attorney::SceneManager::onStartup();
    }
    
    return false;
}

bool SceneManager::onShutdown() {
    if (RenderPassCuller::onShutdown()) {
        return Attorney::SceneManager::onShutdown();
    }

    return false;
}

SceneManager::SceneManager(Kernel& parentKernel)
    : FrameListener("SceneManager", parentKernel.frameListenerMgr(), 2),
      InputAggregatorInterface(),
      KernelComponent(parentKernel)
{
}

SceneManager::~SceneManager()
{
    destroy();
}

Scene& SceneManager::getActiveScene() {
    return _scenePool->activeScene();
}

const Scene& SceneManager::getActiveScene() const {
    return _scenePool->activeScene();
}

void SceneManager::idle() {
    if (_sceneSwitchTarget._isSet) {
        parent().platformContext().gfx().getRenderer().postFX().setFadeOut(UColour3(0), 1000.0, 0.0);
        if (!switchScene(_sceneSwitchTarget._targetSceneName,
                         _sceneSwitchTarget._unloadPreviousScene,
                         _sceneSwitchTarget._targetViewRect,
                         _sceneSwitchTarget._loadInSeparateThread))
        {
            DIVIDE_UNEXPECTED_CALL();
        }
        WaitForAllTasks(getActiveScene().context(), true, true);
        parent().platformContext().gfx().getRenderer().postFX().setFadeIn(2750.0);
    } else {
        if (_playerQueueDirty) {
            while (!_playerAddQueue.empty()) {
                auto& [targetScene, playerSGN] = _playerAddQueue.front();
                addPlayerInternal(*targetScene, playerSGN);
                _playerAddQueue.pop();
            }
            while (!_playerRemoveQueue.empty()) {
                auto& [targetScene, playerSGN] = _playerRemoveQueue.front();
                removePlayerInternal(*targetScene, playerSGN);
                _playerRemoveQueue.pop();
            }
            _playerQueueDirty = false;
        }

        getActiveScene().idle();
    }
}

bool SceneManager::init(PlatformContext& platformContext, ResourceCache* cache) {
    if (_platformContext == nullptr) {
        _platformContext = &platformContext;
        _resourceCache = cache;
        platformContext.kernel().frameListenerMgr().registerFrameListener(this, 1);

        _recast = eastl::make_unique<AI::Navigation::DivideRecast>();

        _scenePool = MemoryManager_NEW ScenePool(*this);

        _sceneGraphCullTimers[to_U32(RenderStage::DISPLAY)] = &Time::ADD_TIMER(Util::StringFormat("SceneGraph cull timer: Display").c_str());
        _sceneGraphCullTimers[to_U32(RenderStage::REFLECTION)] = &Time::ADD_TIMER(Util::StringFormat("SceneGraph cull timer: Reflection").c_str());
        _sceneGraphCullTimers[to_U32(RenderStage::REFRACTION)] = &Time::ADD_TIMER(Util::StringFormat("SceneGraph cull timer: Refraction").c_str());
        _sceneGraphCullTimers[to_U32(RenderStage::SHADOW)] = &Time::ADD_TIMER(Util::StringFormat("SceneGraph cull timer: Shadow").c_str());

        _sceneData = MemoryManager_NEW SceneShaderData(platformContext.gfx());
        _renderPassCuller = MemoryManager_NEW RenderPassCuller();
        _scenePool->init();

        _init = true;
    } else {
        _init = false;
    }
    return _init;
}

void SceneManager::destroy() {
    if (_init) {
        Vegetation::destroyStaticData();
        MemoryManager::SAFE_DELETE(_sceneData);
        _platformContext->kernel().frameListenerMgr().removeFrameListener(this);
        Console::printfn(Locale::get(_ID("STOP_SCENE_MANAGER")));
        // Console::printfn(Locale::get("SCENE_MANAGER_DELETE"));
        Console::printfn(Locale::get(_ID("SCENE_MANAGER_REMOVE_SCENES")));
        MemoryManager::DELETE(_scenePool);
        MemoryManager::DELETE(_renderPassCuller);
        _recast.reset();
        _platformContext = nullptr;
        _init = false;
    }
}

Scene* SceneManager::load(const Str256& sceneName) {
    bool foundInCache = false;
    Scene* loadingScene = _scenePool->getOrCreateScene(*_platformContext, parent().resourceCache(), *this, sceneName, foundInCache);

    if (!loadingScene) {
        Console::errorfn(Locale::get(_ID("ERROR_XML_LOAD_INVALID_SCENE")));
        return nullptr;
    }

    _platformContext->paramHandler().setParam(_ID("currentScene"), stringImpl(sceneName.c_str()));

    const bool sceneNotLoaded = loadingScene->getState() != ResourceState::RES_LOADED;

    if (sceneNotLoaded) {
        
        // Load the main scene from XML
        if (!Attorney::SceneManager::loadXML(*loadingScene, sceneName)) {
            return nullptr;
        }

        if (Attorney::SceneManager::load(*loadingScene, sceneName)) {
            Attorney::SceneManager::postLoad(*loadingScene);
        } else {
            return nullptr;
        }
    }

    return loadingScene;
}

bool SceneManager::unloadScene(Scene* scene) const {
    assert(scene != nullptr);
    if (_saveTask != nullptr) {
        Wait(*_saveTask);
    }

    _platformContext->gui().onUnloadScene(scene);
    Attorney::SceneManager::onRemoveActive(*scene);
    return Attorney::SceneManager::unload(*scene);
}

void SceneManager::setActiveScene(Scene* const scene) {
    assert(scene != nullptr);
    if (_saveTask != nullptr) {
        Wait(*_saveTask);
    }

    Attorney::SceneManager::onRemoveActive(_scenePool->defaultSceneActive() ? _scenePool->defaultScene()
                                                                            : getActiveScene());

    _scenePool->activeScene(*scene);
    Attorney::SceneManager::onSetActive(*scene);
    if (!LoadSave::loadScene(*scene)) {
        //corrupt save
    }

    _platformContext->gui().onChangeScene(scene);
    _platformContext->paramHandler().setParam(_ID("activeScene"), scene->resourceName());
}

bool SceneManager::switchScene(const Str256& name, bool unloadPrevious, const Rect<U16>& targetRenderViewport, const bool threaded) {
    assert(!name.empty());

    Scene* sceneToUnload = nullptr;
    if (!_scenePool->defaultSceneActive()) {
        sceneToUnload = &_scenePool->activeScene();
    }

    // We use our rendering task pool for scene changes because we might be creating / loading GPU assets (shaders, textures, buffers, etc)
    Start(*CreateTask(_platformContext->taskPool(TaskPoolType::HIGH_PRIORITY),
        [this, name, unloadPrevious, &sceneToUnload](const Task& /*parentTask*/)
        {
            // Load first, unload after to make sure we don't reload common resources
            if (load(name) != nullptr) {
                if (unloadPrevious && sceneToUnload) {
                    Attorney::SceneManager::onRemoveActive(*sceneToUnload);
                    unloadScene(sceneToUnload);
                }
            }
        }),
        threaded ? TaskPriority::DONT_CARE : TaskPriority::REALTIME, 
        [this, name, targetRenderViewport, unloadPrevious, &sceneToUnload]()
        {
            bool foundInCache = false;
            Scene* loadedScene = _scenePool->getOrCreateScene(*_platformContext, parent().resourceCache(), *this, name, foundInCache);
            assert(loadedScene != nullptr && foundInCache);

            if(loadedScene->getState() == ResourceState::RES_LOADING) {
                Attorney::SceneManager::postLoadMainThread(*loadedScene, targetRenderViewport);
                if (loadedScene->getGUID() != _scenePool->defaultScene().getGUID())
                {
                    SceneGUIElements* gui = Attorney::SceneManager::gui(*loadedScene);
                    GUIButton* btn = gui->addButton("Back",
                                                    "Back",
                                                    pixelPosition(15, 15),
                                                    pixelScale(50, 25));
                    
                    btn->setEventCallback(GUIButton::Event::MouseClick,
                        [this, &targetRenderViewport](I64 /*btnGUID*/)
                        {
                            _sceneSwitchTarget = {
                                _scenePool->defaultScene().resourceName(),
                                targetRenderViewport, 
                                true,
                                false,
                                true
                            };
                        });
                }
            }
            assert(loadedScene->getState() == ResourceState::RES_LOADED);
            setActiveScene(loadedScene);

            if (unloadPrevious) {
                _scenePool->deleteScene(sceneToUnload);
            }

            _renderPassCuller->clear();
            _parent.platformContext().app().timer().resetFPSCounter();
            
        });

    _sceneSwitchTarget = {};

    return true;
}

vectorEASTL<Str256> SceneManager::sceneNameList(const bool sorted) const {
    return _scenePool->sceneNameList(sorted);
}

void SceneManager::initPostLoadState() noexcept {
    _processInput = true;
}

void SceneManager::onSizeChange(const SizeChangeParams& params) {
    if (params.isWindowResize) {
        return;
    }
    const U16 w = params.width;
    const U16 h = params.height;

    const F32 aspectRatio = to_F32(w) / h;

    if (_init) {
        
        const F32 fov = _platformContext->config().runtime.verticalFOV;
        const vec2<F32> zPlanes(Camera::s_minNearZ, _platformContext->config().runtime.cameraViewDistance);

        for (const UnitComponent* player : _players) {
            if (player != nullptr) {
                player->getUnit<Player>()->camera()->setProjection(aspectRatio, fov, zPlanes);
            }
        }

        Camera::utilityCamera(Camera::UtilityCamera::DEFAULT)->setProjection(aspectRatio, fov, zPlanes);
    }
}

void SceneManager::addPlayer(Scene& parentScene, SceneGraphNode* playerNode, const bool queue) {
    if (queue) {
        _playerAddQueue.push(std::make_pair(&parentScene, playerNode));
        _playerQueueDirty = true;
    } else {
        addPlayerInternal(parentScene, playerNode);
    }
}

void SceneManager::addPlayerInternal(Scene& parentScene, SceneGraphNode* playerNode) {
    const I64 sgnGUID = playerNode->getGUID();
    for (const UnitComponent* crtPlayer : _players) {
        if (crtPlayer && crtPlayer->getSGN()->getGUID() == sgnGUID) {
            return;
        }
    }

    U32 i = 0;
    for (; i < Config::MAX_LOCAL_PLAYER_COUNT; ++i) {
        if (_players[i] == nullptr) {
            break;
        }
    }

    if (i < Config::MAX_LOCAL_PLAYER_COUNT) {
        const Player_ptr player = std::make_shared<Player>(to_U8(i), parent().frameListenerMgr(), 666 + i);
        player->camera()->fromCamera(*Camera::utilityCamera(Camera::UtilityCamera::DEFAULT));
        player->camera()->setFixedYawAxis(true);
        _players[i] = playerNode->get<UnitComponent>();
        _players[i]->setUnit(player);

        ++_activePlayerCount;

        Attorney::SceneManager::onPlayerAdd(parentScene, player);
    }
}

void SceneManager::removePlayer(Scene& parentScene, SceneGraphNode* playerNode, const bool queue) {
    if (queue) {
        _playerRemoveQueue.push(std::make_pair(&parentScene, playerNode));
        _playerQueueDirty = true;
    } else {
        removePlayerInternal(parentScene, playerNode);
    }
}

void SceneManager::removePlayerInternal(Scene& parentScene, SceneGraphNode* playerNode) {
    if (playerNode == nullptr) {
        return;
    }

    const I64 targetGUID = playerNode->getGUID();
    for (U32 i = 0; i < Config::MAX_LOCAL_PLAYER_COUNT; ++i) {
        if (_players[i] != nullptr && _players[i]->getSGN()->getGUID() == targetGUID) {
            --_activePlayerCount;
            Attorney::SceneManager::onPlayerRemove(parentScene, _players[i]->getUnit<Player>());
            _players[i] = nullptr;
            break;
        }
    }
}

vectorEASTL<SceneGraphNode*> SceneManager::getNodesInScreenRect(const Rect<I32>& screenRect, const Camera& camera, const Rect<I32>& viewport) const {
    OPTICK_EVENT();
    constexpr std::array<SceneNodeType, 6> s_ignoredNodes = {
        SceneNodeType::TYPE_TRANSFORM,
        SceneNodeType::TYPE_WATER,
        SceneNodeType::TYPE_SKY,
        SceneNodeType::TYPE_PARTICLE_EMITTER,
        SceneNodeType::TYPE_INFINITEPLANE,
        SceneNodeType::TYPE_VEGETATION
    };
    static vectorEASTL<SGNRayResult> rayResults = {};

    const SceneGraph* sceneGraph = getActiveScene().sceneGraph();
    const vec3<F32>& eye = camera.getEye();
    const vec2<F32>& zPlanes = camera.getZPlanes();

    SGNIntersectionParams intersectionParams = {};
    intersectionParams._includeTransformNodes = false;
    intersectionParams._ignoredTypes = s_ignoredNodes.data();
    intersectionParams._ignoredTypesCount = s_ignoredNodes.size();

    const auto CheckPointLoS = [&](const vec3<F32>& point, const I64 nodeGUID, const I64 parentNodeGUID) -> bool {
        intersectionParams._ray = { point, point.direction(eye) };
        intersectionParams._range = { 0.f, zPlanes.y };

        const F32 distanceToPoint = eye.distance(point);

        sceneGraph->intersect(intersectionParams, rayResults);

        for (const SGNRayResult& result : rayResults) {
            if (result.sgnGUID == nodeGUID || 
                result.sgnGUID == parentNodeGUID)
            {
                continue;
            }

            if (result.dist < distanceToPoint) {
                return false;
            }
        }
        return true;
    };

    const auto HasLoSToCamera = [&](SceneGraphNode* node, const vec3<F32>& point) {
        I64 parentNodeGUID = -1;
        const I64 nodeGUID = node->getGUID();
        if (node->getNode().type() == SceneNodeType::TYPE_OBJECT3D) {
            parentNodeGUID = node->parent()->getGUID();
        }
        return CheckPointLoS(point, nodeGUID, parentNodeGUID);
    };

    const auto IsNodeInRect = [&screenRect, &camera, &viewport](SceneGraphNode* node) {
        assert(node != nullptr);
        const SceneNode& sNode = node->getNode();
        if (sNode.type() == SceneNodeType::TYPE_OBJECT3D) {
            auto* sComp = node->get<SelectionComponent>();
            if (sComp == nullptr && 
                (sNode.type() == SceneNodeType::TYPE_OBJECT3D && node->getNode<Object3D>().getObjectType() == to_base(ObjectType::SUBMESH)))
            {
                if (node->parent() != nullptr) {
                    // Already selected. Skip.
                    if (node->parent()->hasFlag(SceneGraphNode::Flags::SELECTED)) {
                        return false;
                    }
                    sComp = node->parent()->get<SelectionComponent>();
                }
            }
            if (sComp != nullptr && sComp->enabled()) {
                const BoundsComponent* bComp = node->get<BoundsComponent>();
                if (bComp != nullptr) {
                    const vec3<F32>& center = bComp->getBoundingSphere().getCenter();
                    return screenRect.contains(camera.project(center, viewport));
                }
            }
        }

        return false;
    };

    //Step 1: Grab ALL nodes in rect
    vectorEASTL<SceneGraphNode*> ret = {};

    VisibleNodeList<1024> inRectList;
    const VisibleNodeList<>& visNodes = _renderPassCuller->getNodeCache(RenderStage::DISPLAY);
    for (size_t i = 0; i < visNodes.size(); ++i) {
        const VisibleNode& node = visNodes.node(i);
        if (IsNodeInRect(node._node)) {
            inRectList.append(node);
        }
    }

    //Step 2: Check Straight LoS to camera
    VisibleNodeList<1024> LoSList;
    for (size_t i = 0; i < inRectList.size(); ++i) {
        const VisibleNode& node = inRectList.node(i);
        if (HasLoSToCamera(node._node, node._node->get<BoundsComponent>()->getBoundingSphere().getCenter())) {
            LoSList.append(node);
        } else {
            // This is gonna hurt.The raycast failed, but the node might still be visible
            const OBB& obb = node._node->get<BoundsComponent>()->getOBB();
            for (U8 p = 0; p < 8; ++p) {
                if (HasLoSToCamera(node._node, obb.cornerPoint(p))) {
                    LoSList.append(node);
                }
            }
        }
    }

    //Step 3: Create list of visible nodes
    for (size_t i = 0; i < LoSList.size(); ++i) {
        SceneGraphNode* parsedNode = LoSList.node(i)._node;
        if (parsedNode != nullptr) {
            while (true) {
                const SceneNode& node = parsedNode->getNode();
                if (node.type() == SceneNodeType::TYPE_OBJECT3D && static_cast<const Object3D&>(node).getObjectType()._value == ObjectType::SUBMESH) {
                    parsedNode = parsedNode->parent();
                } else {
                    break;
                }
            }

            if (eastl::find(cbegin(ret), cend(ret), parsedNode) == cend(ret)) {
                ret.push_back(parsedNode);
            }
        }
    }

    return ret;
}

bool SceneManager::frameStarted(const FrameEvent& evt) {
    OPTICK_EVENT();

    _sceneData->uploadToGPU();

    return Attorney::SceneManager::frameStarted(getActiveScene());
}

bool SceneManager::frameEnded(const FrameEvent& evt) {
    OPTICK_EVENT();

    return Attorney::SceneManager::frameEnded(getActiveScene());
}

void SceneManager::updateSceneState(const U64 deltaTimeUS) {
    OPTICK_EVENT();

    Scene& activeScene = getActiveScene();
    assert(activeScene.getState() == ResourceState::RES_LOADED);
    // Update internal timers
    _elapsedTime += deltaTimeUS;
    _elapsedTimeMS = Time::MicrosecondsToMilliseconds<U32>(_elapsedTime);

    FogDescriptor& fog = activeScene.state()->renderState().fogDescriptor();
    const bool fogEnabled = _platformContext->config().rendering.enableFog;
    if (fog.dirty() || fogEnabled != fog.active()) {
        const vec3<F32>& colour = fog.colour();
        const F32 density = fogEnabled ? fog.density() : 0.0f;
        _sceneData->fogDetails(colour.r, colour.g, colour.b, density);
        fog.clean();
        fog.active(fogEnabled);
    }

    vec3<F32> sunDirection = {0.0f, -0.75f, -0.75f};
    FColour3 sunColour = DefaultColours::WHITE;

    const Scene::DayNightData& dayNightData = activeScene.dayNightData();
    if (dayNightData._skyInstance != nullptr) {
        sunDirection = dayNightData._skyInstance->getCurrentDetails()._eulerDirection;
        sunDirection = DirectionFromEuler(sunDirection, WORLD_Z_NEG_AXIS);
    }
    if (dayNightData._dirLight != nullptr) {
        sunColour = dayNightData._dirLight->getDiffuseColour();
    }
    _sceneData->sunDetails(sunDirection, sunColour);

    //_sceneData->skyColour(horizonColour, zenithColour);

    const SceneState* activeSceneState = activeScene.state();
    _sceneData->windDetails(activeSceneState->windDirX(),
                            0.0f,
                            activeSceneState->windDirZ(),
                            activeSceneState->windSpeed());

    _sceneData->shadowingSettings(activeSceneState->lightBleedBias(), activeSceneState->minShadowVariance());
    activeScene.updateSceneState(deltaTimeUS);

    U8 index = 0;

    const vectorEASTL<WaterBodyData>& waterBodies = activeSceneState->waterBodies();
    for (const auto& body : waterBodies) {
        _sceneData->waterDetails(index++, body);
    }
    _saveTimer += deltaTimeUS;

    if (_saveTimer >= Time::SecondsToMicroseconds(Config::Build::IS_DEBUG_BUILD ? 5 : 10)) {
        saveActiveScene(true, true);
        _saveTimer = 0ULL;
    }
}

void SceneManager::drawCustomUI(const Rect<I32>& targetViewport, GFX::CommandBuffer& bufferInOut) {
    //Set a 2D camera for rendering
    EnqueueCommand(bufferInOut, GFX::SetCameraCommand{ Camera::utilityCamera(Camera::UtilityCamera::_2D)->snapshot() });
    EnqueueCommand(bufferInOut, GFX::SetViewportCommand{ targetViewport });

    Attorney::SceneManager::drawCustomUI(getActiveScene(), targetViewport, bufferInOut);
}

void SceneManager::debugDraw(const RenderStagePass& stagePass, const Camera* camera, GFX::CommandBuffer& bufferInOut) {
    OPTICK_EVENT();

    Scene& activeScene = getActiveScene();

    Attorney::SceneManager::debugDraw(activeScene, camera, stagePass, bufferInOut);
    // Draw bounding boxes, skeletons, axis gizmo, etc.
    _platformContext->gfx().debugDraw(activeScene.renderState(), camera, bufferInOut);
}

Camera* SceneManager::playerCamera(const PlayerIndex idx) const {
    if (getActivePlayerCount() <= idx) {
        return nullptr;
    }

    Camera* overrideCamera = getActiveScene().state()->playerState(idx).overrideCamera();
    if (overrideCamera == nullptr) {
        overrideCamera = _players[idx]->getUnit<Player>()->camera();
    }

    return overrideCamera;
}

Camera* SceneManager::playerCamera() const {
    return playerCamera(_currentPlayerPass);
}

void SceneManager::currentPlayerPass(const PlayerIndex idx) {
    OPTICK_EVENT();

    _currentPlayerPass = idx;
    Attorney::SceneManager::currentPlayerPass(getActiveScene(), _currentPlayerPass);
    playerCamera()->updateLookAt();
}

void SceneManager::moveCameraToNode(const SceneGraphNode* targetNode) const {
    vec3<F32> targetPos = VECTOR3_ZERO;

    /// Root node just means a teleport to (0,0,0)
    if (targetNode->parent() != nullptr) {
        targetPos = playerCamera()->getEye();
        const BoundsComponent* bComp = targetNode->get<BoundsComponent>();
        if (bComp != nullptr) {
            const BoundingSphere& bSphere = bComp->getBoundingSphere();
            targetPos = bSphere.getCenter();
            targetPos -= bSphere.getRadius() * 1.5f * playerCamera()->getForwardDir();
        } else {
            const TransformComponent* tComp = targetNode->get<TransformComponent>();
            if (tComp != nullptr) {
                targetPos = tComp->getPosition();
                targetPos -= playerCamera()->getForwardDir() * 3.0f;
            }
        }
    }

    playerCamera()->setEye(targetPos);
}

bool SceneManager::saveNode(const SceneGraphNode* targetNode) const {
    return LoadSave::saveNodeToXML(getActiveScene(), targetNode);
}

bool SceneManager::loadNode(SceneGraphNode* targetNode) const {
    return LoadSave::loadNodeFromXML(getActiveScene(), targetNode);
}

void SceneManager::getSortedReflectiveNodes(const Camera* camera, const RenderStage stage, const bool inView, VisibleNodeList<>& nodesOut) const {
    OPTICK_EVENT();

    static vectorEASTL<SceneGraphNode*> allNodes = {};
    getActiveScene().sceneGraph()->getNodesByType({ SceneNodeType::TYPE_WATER, SceneNodeType::TYPE_OBJECT3D }, allNodes);

    erase_if(allNodes,
             [](SceneGraphNode* node) noexcept ->  bool {
                const Material_ptr& mat = node->get<RenderingComponent>()->getMaterialInstance();
                return node->getNode().type() != SceneNodeType::TYPE_WATER && (mat == nullptr || !mat->reflective());
             });

    if (inView) {
        NodeCullParams cullParams = {};
        cullParams._lodThresholds = getActiveScene().state()->renderState().lodThresholds();
        cullParams._stage = stage;
        cullParams._currentCamera = camera;
        cullParams._cullMaxDistanceSq = SQUARED(camera->getZPlanes().y);

        _renderPassCuller->frustumCull(cullParams, allNodes, nodesOut);
    } else {
        _renderPassCuller->toVisibleNodes(camera, allNodes, nodesOut);
    }
}

void SceneManager::getSortedRefractiveNodes(const Camera* camera, const RenderStage stage, const bool inView, VisibleNodeList<>& nodesOut) const {
    OPTICK_EVENT();

    static vectorEASTL<SceneGraphNode*> allNodes = {};
    getActiveScene().sceneGraph()->getNodesByType({ SceneNodeType::TYPE_WATER, SceneNodeType::TYPE_OBJECT3D }, allNodes);

    erase_if(allNodes,
             [](SceneGraphNode* node) noexcept ->  bool {
                  const Material_ptr& mat = node->get<RenderingComponent>()->getMaterialInstance();
                  return node->getNode().type() != SceneNodeType::TYPE_WATER && (mat == nullptr || !mat->refractive());
             });
    if (inView) {
        NodeCullParams cullParams = {};
        cullParams._lodThresholds = getActiveScene().state()->renderState().lodThresholds();
        cullParams._stage = stage;
        cullParams._currentCamera = camera;
        cullParams._cullMaxDistanceSq = SQUARED(camera->getZPlanes().y);

        _renderPassCuller->frustumCull(cullParams, allNodes, nodesOut);
    } else {
        _renderPassCuller->toVisibleNodes(camera, allNodes, nodesOut);
    }
}

void SceneManager::initDefaultCullValues(const RenderStage stage, NodeCullParams& cullParamsInOut) {
    Scene& activeScene = getActiveScene();
    SceneState* sceneState = activeScene.state();

    cullParamsInOut._stage = stage;
    cullParamsInOut._lodThresholds = sceneState->renderState().lodThresholds(stage);
    if (stage != RenderStage::SHADOW) {
        cullParamsInOut._cullMaxDistanceSq = SQUARED(sceneState->renderState().generalVisibility());
    } else {
        cullParamsInOut._cullMaxDistanceSq = std::numeric_limits<F32>::max();
    }
}

VisibleNodeList<>& SceneManager::cullSceneGraph(const NodeCullParams& params) {
    OPTICK_EVENT();

    Time::ScopedTimer timer(*_sceneGraphCullTimers[to_U32(params._stage)]);

    Scene& activeScene = getActiveScene();
    SceneState* sceneState = activeScene.state();

     return _renderPassCuller->frustumCull(params, activeScene.sceneGraph(), sceneState, _parent.platformContext());
}

void SceneManager::prepareLightData(const RenderStage stage, const vec3<F32>& cameraPos, const mat4<F32>& viewMatrix) {
    OPTICK_EVENT();

    if (stage != RenderStage::SHADOW) {
        getActiveScene().lightPool().prepareLightData(stage, cameraPos, viewMatrix);
    }
}

void SceneManager::onLostFocus() {
    if (!_init) {
        return;
    }

    getActiveScene().onLostFocus();
}

void SceneManager::onGainFocus() {
    if (!_init) {
        return;
    }

    getActiveScene().onGainFocus();
}

void SceneManager::resetSelection(const PlayerIndex idx) {
    Attorney::SceneManager::resetSelection(getActiveScene(), idx);
    for (auto& cbk : _selectionChangeCallbacks) {
        cbk(idx, {});
    }
}

void SceneManager::setSelected(const PlayerIndex idx, const vectorEASTL<SceneGraphNode*>& SGNs, const bool recursive) {
    Attorney::SceneManager::setSelected(getActiveScene(), idx, SGNs, recursive);
    for (auto& cbk : _selectionChangeCallbacks) {
        cbk(idx, SGNs);
    }
}

void SceneManager::onNodeDestroy(SceneGraphNode* node) {
    ACKNOWLEDGE_UNUSED(node);

    for (PlayerIndex p = 0; p < _activePlayerCount; ++p) {
        resetSelection(p);
    }
}

void SceneManager::mouseMovedExternally(const Input::MouseMoveEvent& arg) {
    Attorney::SceneManager::clearHoverTarget(getActiveScene(), arg);
}

SceneNode_ptr SceneManager::createNode(const SceneNodeType type, const ResourceDescriptor& descriptor) {
    return Attorney::SceneManager::createNode(getActiveScene(), type, descriptor);
}

SceneEnvironmentProbePool* SceneManager::getEnvProbes() const {
    return Attorney::SceneManager::getEnvProbes(getActiveScene());
}
///--------------------------Input Management-------------------------------------///

bool SceneManager::onKeyDown(const Input::KeyEvent& key) {
    if (!_processInput) {
        return false;
    }

    return getActiveScene().input()->onKeyDown(key);
}

bool SceneManager::onKeyUp(const Input::KeyEvent& key) {
    if (!_processInput) {
        return false;
    }

    return getActiveScene().input()->onKeyUp(key);
}

bool SceneManager::mouseMoved(const Input::MouseMoveEvent& arg) {
    if (!_processInput) {
        return false;

    }

    return getActiveScene().input()->mouseMoved(arg);
}

bool SceneManager::mouseButtonPressed(const Input::MouseButtonEvent& arg) {
    if (!_processInput) {
        return false;
    }

    return getActiveScene().input()->mouseButtonPressed(arg);
}

bool SceneManager::mouseButtonReleased(const Input::MouseButtonEvent& arg) {
    if (!_processInput) {
        return false;
    }

    return getActiveScene().input()->mouseButtonReleased(arg);
}

bool SceneManager::joystickAxisMoved(const Input::JoystickEvent& arg) {
    if (!_processInput) {
        return false;
    }

    return getActiveScene().input()->joystickAxisMoved(arg);
}

bool SceneManager::joystickPovMoved(const Input::JoystickEvent& arg) {
    if (!_processInput) {
        return false;
    }

    return getActiveScene().input()->joystickPovMoved(arg);
}

bool SceneManager::joystickButtonPressed(const Input::JoystickEvent& arg) {
    if (!_processInput) {
        return false;
    }

    return getActiveScene().input()->joystickButtonPressed(arg);
}

bool SceneManager::joystickButtonReleased(const Input::JoystickEvent& arg) {
    if (!_processInput) {
        return false;
    }

    return getActiveScene().input()->joystickButtonReleased(arg);
}

bool SceneManager::joystickBallMoved(const Input::JoystickEvent& arg) {
    if (!_processInput) {
        return false;
    }

    return getActiveScene().input()->joystickBallMoved(arg);
}

bool SceneManager::joystickAddRemove(const Input::JoystickEvent& arg) {
    if (!_processInput) {
        return false;
    }

    return getActiveScene().input()->joystickAddRemove(arg);
}

bool SceneManager::joystickRemap(const Input::JoystickEvent &arg) {
    if (!_processInput) {
        return false;
    }

    return getActiveScene().input()->joystickRemap(arg);
}

bool SceneManager::onUTF8(const Input::UTF8Event& arg) {
    ACKNOWLEDGE_UNUSED(arg);

    return false;
}

bool LoadSave::loadScene(Scene& activeScene) {
    if (activeScene.state()->saveLoadDisabled()) {
        return true;
    }

    const Str256& sceneName = activeScene.resourceName();

    const ResourcePath path = Paths::g_saveLocation +  sceneName + "/";
    const ResourcePath saveFile = ResourcePath("current_save.sav");
    const ResourcePath bakSaveFile = ResourcePath("save.bak");

    bool isLoadFromBackup = false;
    // If file is missing, restore from bak
    if (!fileExists(path + saveFile)) {
        isLoadFromBackup = true;

        // Save file might be deleted if it was corrupted
        if (fileExists(path + bakSaveFile)) {
            if (!copyFile(path, bakSaveFile, path, saveFile, false)) {
                NOP();
            }
        }
    }

    ByteBuffer save;
    if (save.loadFromFile(path.c_str(), saveFile.c_str())) {
        if (!Attorney::SceneLoadSave::load(activeScene, save)) {
            //Remove the save and try the backup
            if (!deleteFile(path, saveFile)) {
                NOP();
            }
            if (!isLoadFromBackup) {
                return loadScene(activeScene);
            }
        }
    }
    return false;
}


bool LoadSave::saveNodeToXML(const Scene& activeScene, const SceneGraphNode* node) {
    return activeScene.saveNodeToXML(node);
}

bool LoadSave::loadNodeFromXML(const Scene& activeScene, SceneGraphNode* node) {
    return activeScene.loadNodeFromXML(node);
}

bool LoadSave::saveScene(const Scene& activeScene, const bool toCache, const DELEGATE<void, std::string_view>& msgCallback, const DELEGATE<void, bool>& finishCallback) {
    if (!toCache) {
        return activeScene.saveXML(msgCallback, finishCallback);
    }

    bool ret = false;
    if (activeScene.state()->saveLoadDisabled()) {
        ret = true;
    } else {
        const Str256& sceneName = activeScene.resourceName();
        const ResourcePath path = Paths::g_saveLocation + sceneName + "/";
        const ResourcePath saveFile = ResourcePath("current_save.sav");
        const ResourcePath bakSaveFile = ResourcePath("save.bak");

        if (fileExists(path + saveFile)) {
            if (!copyFile(path, saveFile, path, bakSaveFile, true)) {
                return false;
            }
        }

        ByteBuffer save;
        if (Attorney::SceneLoadSave::save(activeScene, save)) {
            ret = save.dumpToFile(path.c_str(), saveFile.c_str());
            assert(ret);
        }
    }
    if (finishCallback) {
        finishCallback(ret);
    }
    return ret;
}

bool SceneManager::saveActiveScene(bool toCache, const bool deferred, const DELEGATE<void, std::string_view>& msgCallback, const DELEGATE<void, bool>& finishCallback) {
    OPTICK_EVENT();

    const Scene& activeScene = getActiveScene();

    if (_saveTask != nullptr) {
        if (!Finished(*_saveTask)) {
            if (toCache) {
                return false;
            }
            if_constexpr(Config::Build::IS_DEBUG_BUILD) {
                DebugBreak();
            }
        }
        Wait(*_saveTask);
    }

    TaskPool& pool = parent().platformContext().taskPool(TaskPoolType::LOW_PRIORITY);
    _saveTask = CreateTask(pool,
                           nullptr,
                           [&activeScene, msgCallback, finishCallback, toCache](const Task& /*parentTask*/) {
                               LoadSave::saveScene(activeScene, toCache, msgCallback, finishCallback);
                           },
                           false);
    Start(*_saveTask, deferred ? TaskPriority::DONT_CARE : TaskPriority::REALTIME);

    return true;
}

bool SceneManager::networkUpdate(U32 frameCount) {
    ACKNOWLEDGE_UNUSED(frameCount);

    return true;
}

};