#include "Headers/SceneManager.h"
#include "Headers/AIManager.h"

#include "SceneList.h"
#include "Core/Headers/ParamHandler.h"
#include "Geometry/Importer/Headers/DVDConverter.h"
#include "Rendering/RenderPass/Headers/RenderQueue.h"
#include "Rendering/RenderPass/Headers/RenderPassCuller.h"

namespace Divide {

SceneManager::SceneManager()
    : FrameListener(),
      _GUI(nullptr),
      _activeScene(nullptr),
      _renderPassCuller(nullptr),
      _renderPassManager(nullptr),
      _defaultMaterial(nullptr),
      _loadPreRenderComplete(false),
      _processInput(false),
      _init(false)

{
    AI::AIManager::createInstance();
}

SceneManager::~SceneManager() {
    UNREGISTER_FRAME_LISTENER(&(this->getInstance()));

    Console::printfn(Locale::get("STOP_SCENE_MANAGER"));
    // Console::printfn(Locale::get("SCENE_MANAGER_DELETE"));
    Console::printfn(Locale::get("SCENE_MANAGER_REMOVE_SCENES"));
    MemoryManager::DELETE_HASHMAP(_sceneMap);
    MemoryManager::DELETE(_renderPassCuller);
    // Destroy the model loader;
    DVDConverter::getInstance().destroyInstance();
}

bool SceneManager::init(GUI* const gui) {
    REGISTER_FRAME_LISTENER(&(this->getInstance()), 1);

    // Load default material
    Console::printfn(Locale::get("LOAD_DEFAULT_MATERIAL"));
    _defaultMaterial = XML::loadMaterialXML(
        ParamHandler::getInstance().getParam<std::string>("scriptLocation") +
            "/defaultMaterial",
        false);
    _defaultMaterial->dumpToFile(false);

    _GUI = gui;
    _renderPassCuller = MemoryManager_NEW RenderPassCuller();
    _renderPassManager = &RenderPassManager::getInstance();
    _init = true;
    return true;
}

bool SceneManager::load(const stringImpl& sceneName,
                        const vec2<U16>& resolution) {
    assert(_init == true && _GUI != nullptr);
    Console::printfn(Locale::get("SCENE_MANAGER_LOAD_SCENE_DATA"));
    // Initialize the model importer:
    if (!DVDConverter::getInstance().init()) {
        return false;
    }
    XML::loadScene(stringAlg::fromBase(sceneName), *this);
    if (!_activeScene) {
        return false;
    }
    cacheResolution(resolution);
    return Attorney::SceneManager::load(*_activeScene, sceneName, _GUI);
}

Scene* SceneManager::createScene(const stringImpl& name) {
    Scene* scene = nullptr;

    if (!name.empty()) {
        scene = _sceneFactory[name]();
    }

    if (scene != nullptr) {
        hashAlg::emplace(_sceneMap, name, scene);
    }

    return scene;
}

bool SceneManager::unloadCurrentScene() {
    AI::AIManager::getInstance().pauseUpdate(true);
    RemoveResource(_defaultMaterial);
    return Attorney::SceneManager::unload(*_activeScene);
}

void SceneManager::initPostLoadState() {
    Material::serializeShaderLoad(true);
    _loadPreRenderComplete = _processInput = true;
}

bool SceneManager::deinitializeAI(bool continueOnErrors) {
    bool state =
        Attorney::SceneManager::deinitializeAI(*_activeScene, continueOnErrors);
    AI::AIManager::getInstance().destroyInstance();
    return state;
}

bool SceneManager::frameStarted(const FrameEvent& evt) {
    return Attorney::SceneManager::frameStarted(*_activeScene);
}

bool SceneManager::framePreRenderStarted(const FrameEvent& evt) {
    return true;
}

bool SceneManager::frameEnded(const FrameEvent& evt) {
    _renderPassCuller->refresh();
     RenderQueue::getInstance().refresh(true);
    if (_loadPreRenderComplete) {
        Material::unlockShaderQueue();
    }
    return Attorney::SceneManager::frameEnded(*_activeScene);
}

void SceneManager::preRender() {
    _activeScene->preRender();
}

void SceneManager::sortVisibleNodes(
    RenderPassCuller::VisibleNodeCache& nodes) const {
    if (nodes._sorted) {
        return;
    }

    RenderPassCuller::VisibleNodeList& visibleNodes = nodes._visibleNodes;
    std::sort(std::begin(visibleNodes), std::end(visibleNodes),
              [](const RenderPassCuller::RenderableNode& nodeA,
                 const RenderPassCuller::RenderableNode& nodeB) {
                  RenderingComponent* renderableA =
                      nodeA._visibleNode->getComponent<RenderingComponent>();
                  RenderingComponent* renderableB =
                      nodeB._visibleNode->getComponent<RenderingComponent>();
                  return renderableA->drawOrder() < renderableB->drawOrder();
              });

    nodes._sorted = true;
}

void SceneManager::updateVisibleNodes() {
    auto cullingFunction = [](SceneGraphNode* node) -> bool {
        if (node->getNode()->getType() == SceneNodeType::TYPE_OBJECT3D) {
            Object3D::ObjectType type =
                node->getNode<Object3D>()->getObjectType();
            return (type == Object3D::ObjectType::MESH ||
                    type == Object3D::ObjectType::FLYWEIGHT);
        }
        return false;
    };

    RenderQueue& queue = RenderQueue::getInstance();
    RenderPassCuller::VisibleNodeCache& nodes =
        _renderPassCuller->frustumCull(
            GET_ACTIVE_SCENEGRAPH().getRoot(), _activeScene->state(),
            cullingFunction);

    if (!nodes._locked) {
        vec3<F32> eyePos(_activeScene->renderState().getCameraConst().getEye());
        for (RenderPassCuller::RenderableNode& node : nodes._visibleNodes) {
            queue.addNodeToQueue(*node._visibleNode, eyePos);
        }
        queue.sort(GFX_DEVICE.getRenderStage());
        sortVisibleNodes(nodes);

        nodes = _renderPassCuller->occlusionCull(nodes);
    }

    GFX_DEVICE.buildDrawCommands(nodes._visibleNodes,
                                 _activeScene->renderState(),
                                 !nodes._locked);
}

void SceneManager::renderVisibleNodes() {
    updateVisibleNodes();
    _renderPassManager->render(_activeScene->renderState(),
                               _activeScene->getSceneGraph());
}

void SceneManager::render(RenderStage stage, const Kernel& kernel) {
    assert(_activeScene != nullptr);

    static DELEGATE_CBK<> renderFunction;
    if (!renderFunction) {
        if (!_activeScene->renderCallback()) {
            renderFunction =
                DELEGATE_BIND(&SceneManager::renderVisibleNodes, this);
        } else {
            renderFunction = _activeScene->renderCallback();
        }
    }

    _activeScene->renderState().getCamera().renderLookAt();
    Attorney::KernelScene::submitRenderCall(
        kernel, stage, _activeScene->renderState(), renderFunction);
    Attorney::SceneManager::debugDraw(*_activeScene, stage);
}

void SceneManager::postRender() {
    _activeScene->postRender();
}

void SceneManager::onCameraChange() {
    Attorney::SceneManager::onCameraChange(*_activeScene);
}

///--------------------------Input
/// Management-------------------------------------///

bool SceneManager::onKeyDown(const Input::KeyEvent& key) {
    if (!_processInput) {
        return false;
    }
    return _activeScene->onKeyDown(key);
}

bool SceneManager::onKeyUp(const Input::KeyEvent& key) {
    if (!_processInput) {
        return false;
    }

    return _activeScene->onKeyUp(key);
}

bool SceneManager::mouseMoved(const Input::MouseEvent& arg) {
    if (!_processInput) {
        return false;
    }

    return _activeScene->mouseMoved(arg);
}

bool SceneManager::mouseButtonPressed(const Input::MouseEvent& arg,
                                      Input::MouseButton button) {
    if (!_processInput) {
        return false;
    }
    return _activeScene->mouseButtonPressed(arg, button);
}

bool SceneManager::mouseButtonReleased(const Input::MouseEvent& arg,
                                       Input::MouseButton button) {
    if (!_processInput) {
        return false;
    }

    return _activeScene->mouseButtonReleased(arg, button);
}

bool SceneManager::joystickAxisMoved(const Input::JoystickEvent& arg, I8 axis) {
    if (!_processInput) {
        return false;
    }
    return _activeScene->joystickAxisMoved(arg, axis);
}

bool SceneManager::joystickPovMoved(const Input::JoystickEvent& arg, I8 pov) {
    if (!_processInput) {
        return false;
    }
    return _activeScene->joystickPovMoved(arg, pov);
}

bool SceneManager::joystickButtonPressed(const Input::JoystickEvent& arg,
                                         I8 button) {
    if (!_processInput) {
        return false;
    }
    return _activeScene->joystickButtonPressed(arg, button);
}

bool SceneManager::joystickButtonReleased(const Input::JoystickEvent& arg,
                                          I8 button) {
    if (!_processInput) {
        return false;
    }
    return _activeScene->joystickButtonReleased(arg, button);
}

bool SceneManager::joystickSliderMoved(const Input::JoystickEvent& arg,
                                       I8 index) {
    if (!_processInput) {
        return false;
    }
    return _activeScene->joystickSliderMoved(arg, index);
}

bool SceneManager::joystickVector3DMoved(const Input::JoystickEvent& arg,
                                         I8 index) {
    if (!_processInput) {
        return false;
    }
    return _activeScene->joystickVector3DMoved(arg, index);
}
};