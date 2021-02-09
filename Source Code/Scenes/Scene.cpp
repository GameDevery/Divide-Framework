#include "stdafx.h"

#include "Headers/Scene.h"

#include "Core/Debugging/Headers/DebugInterface.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/EngineTaskPool.h"
#include "Core/Headers/ParamHandler.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Headers/StringHelper.h"
#include "Editor/Headers/Editor.h"

#include "Managers/Headers/SceneManager.h"
#include "Rendering/Camera/Headers/FreeFlyCamera.h"
#include "Rendering/Headers/Renderer.h"
#include "Rendering/PostFX/Headers/PostFX.h"
#include "Utility/Headers/XMLParser.h"

#include "Environment/Sky/Headers/Sky.h"
#include "Environment/Terrain/Headers/InfinitePlane.h"
#include "Environment/Terrain/Headers/Terrain.h"
#include "Environment/Terrain/Headers/TerrainDescriptor.h"
#include "Environment/Water/Headers/Water.h"

#include "Geometry/Material/Headers/Material.h"
#include "Geometry/Shapes/Headers/Mesh.h"
#include "Geometry/Shapes/Predefined/Headers/Box3D.h"
#include "Geometry/Shapes/Predefined/Headers/Quad3D.h"
#include "Geometry/Shapes/Predefined/Headers/Sphere3D.h"

#include "GUI/Headers/GUI.h"
#include "GUI/Headers/GUIConsole.h"

#include "ECS/Components/Headers/DirectionalLightComponent.h"
#include "ECS/Components/Headers/NavigationComponent.h"
#include "ECS/Components/Headers/RigidBodyComponent.h"
#include "ECS/Components/Headers/SelectionComponent.h"
#include "ECS/Components/Headers/SpotLightComponent.h"
#include "ECS/Components/Headers/TransformComponent.h"

#include "Dynamics/Entities/Triggers/Headers/Trigger.h"
#include "Dynamics/Entities/Units/Headers/Player.h"
#include "ECS/Components/Headers/UnitComponent.h"

#include "Platform/Audio/Headers/SFXDevice.h"
#include "Platform/File/Headers/FileManagement.h"
#include "Platform/Headers/PlatformRuntime.h"
#include "Platform/Video/Headers/IMPrimitive.h"
#include "Platform/Video/Headers/RenderStateBlock.h"

#include "Physics/Headers/PhysicsSceneInterface.h"

namespace Divide {

namespace {
    constexpr const char* const g_defaultPlayerName = "Player_%d";
}

Scene::Scene(PlatformContext& context, ResourceCache* cache, SceneManager& parent, const Str256& name)
    : Resource(ResourceType::DEFAULT, name),
      PlatformContextComponent(context),
      _parent(parent),
      _resCache(cache),
      _LRSpeedFactor(5.0f)
{
    _sceneTimerUS = 0UL;
    _sceneState = MemoryManager_NEW SceneState(*this);
    _input = MemoryManager_NEW SceneInput(*this);
    _sceneGraph = MemoryManager_NEW SceneGraph(*this);
    _aiManager = MemoryManager_NEW AI::AIManager(*this, _context.taskPool(TaskPoolType::HIGH_PRIORITY));
    _lightPool = MemoryManager_NEW LightPool(*this, _context);
    _envProbePool = MemoryManager_NEW SceneEnvironmentProbePool(*this);

    _GUI = MemoryManager_NEW SceneGUIElements(*this, _context.gui());

    _loadingTasks.store(0);

    _linesPrimitive = _context.gfx().newIMP();
    _linesPrimitive->name("GenericLinePrimitive");

    RenderStateBlock primitiveDescriptor;
    primitiveDescriptor.depthTestEnabled(false);

    PipelineDescriptor pipeDesc;
    pipeDesc._stateHash = primitiveDescriptor.getHash();
    pipeDesc._shaderProgramHandle = ShaderProgram::DefaultShader()->getGUID();
    _linesPrimitive->pipeline(*_context.gfx().newPipeline(pipeDesc));
}

Scene::~Scene()
{
    MemoryManager::DELETE(_sceneState);
    MemoryManager::DELETE(_input);
    MemoryManager::DELETE(_sceneGraph);
    MemoryManager::DELETE(_aiManager);
    MemoryManager::DELETE(_lightPool);
    MemoryManager::DELETE(_envProbePool);
    MemoryManager::DELETE(_GUI);
    for (IMPrimitive*& prim : _octreePrimitives) {
        _context.gfx().destroyIMP(prim);
    }
    if (_linesPrimitive) {
        _context.gfx().destroyIMP(_linesPrimitive);
    }
}

bool Scene::OnStartup(PlatformContext& context) {
    Sky::OnStartup(context);
    return true;
}

bool Scene::OnShutdown(PlatformContext& context) {
    ACKNOWLEDGE_UNUSED(context);
    return true;
}

bool Scene::frameStarted() {
    UniqueLock<Mutex> lk(_perFrameArenaMutex);
    _perFrameArena.clear();

    return true;
}

bool Scene::frameEnded() {
    return true;
}

bool Scene::idle() {  // Called when application is idle
    if (_cookCollisionMeshesScheduled && checkLoadFlag()) {
        if (_context.gfx().frameCount() > 1) {
            _sceneGraph->getRoot()->get<RigidBodyComponent>()->cookCollisionMesh(resourceName().c_str());
            _cookCollisionMeshesScheduled = false;
        }
    }

    LightPool::idle();

    {
        SharedLock<SharedMutex> r_lock(_tasksMutex);
        if (_tasks.empty()) {
            return true;
        }
    }

    UniqueLock<SharedMutex> r_lock(_tasksMutex);
    _tasks.erase(std::remove_if(begin(_tasks),
                                     end(_tasks),
                                     [](Task* handle) -> bool { 
                                              return handle != nullptr && Finished(*handle);
                                          }),
                 end(_tasks));

    return true;
}

void Scene::addMusic(const MusicType type, const Str64& name, const ResourcePath& srcFile) const {
    const auto[musicFile, musicFilePath] = splitPathToNameAndLocation(srcFile);

    ResourceDescriptor music(name);
    music.assetName(musicFile);
    music.assetLocation(musicFilePath);
    music.flag(true);

    insert(state()->music(type), _ID(name.c_str()), CreateResource<AudioDescriptor>(_resCache, music));
}

bool Scene::saveNodeToXML(const SceneGraphNode* node) const {
    return sceneGraph()->saveNodeToXML(node);
}

bool Scene::loadNodeFromXML(SceneGraphNode* node) const {
    const char* assetsFile = "assets.xml";
    return sceneGraph()->loadNodeFromXML(assetsFile, node);
}

bool Scene::saveXML(const DELEGATE<void, std::string_view>& msgCallback, const DELEGATE<void, bool>& finishCallback) const {
    using boost::property_tree::ptree;
    const char* assetsFile = "assets.xml";

    Console::printfn(Locale::get(_ID("XML_SAVE_SCENE_START")), resourceName().c_str());

    const ResourcePath scenePath = Paths::g_xmlDataLocation + Paths::g_scenesLocation;

    const ResourcePath sceneLocation(scenePath + "/" + resourceName().c_str());
    const ResourcePath sceneDataFile(sceneLocation + ".xml");

    if (msgCallback) {
        msgCallback("Validating directory structure ...");
    }

    if (!createDirectory(sceneLocation + "/collisionMeshes/")) {
        NOP();
    }
    if (!createDirectory(sceneLocation + "/navMeshes/")) {
        NOP();
    }
    if (!createDirectory(sceneLocation + "/nodes/")) {
        NOP();
    }

    // A scene does not necessarily need external data files. Data can be added in code for simple scenes
    {
        if (msgCallback) {
            msgCallback("Saving scene settings ...");
        }

        ParamHandler& par = _context.paramHandler();

        ptree pt;
        pt.put("assets", assetsFile);
        pt.put("musicPlaylist", "musicPlaylist.xml");

        pt.put("vegetation.grassVisibility", state()->renderState().grassVisibility());
        pt.put("vegetation.treeVisibility", state()->renderState().treeVisibility());

        pt.put("wind.windDirX", state()->windDirX());
        pt.put("wind.windDirZ", state()->windDirZ());
        pt.put("wind.windSpeed", state()->windSpeed());

        pt.put("options.visibility", state()->renderState().generalVisibility());
        pt.put("options.cameraSpeed.<xmlattr>.move", par.getParam<F32>(_ID((resourceName() + ".options.cameraSpeed.move").c_str())));
        pt.put("options.cameraSpeed.<xmlattr>.turn", par.getParam<F32>(_ID((resourceName() + ".options.cameraSpeed.turn").c_str())));
        pt.put("options.autoCookPhysicsAssets", true);

        pt.put("fog.fogDensityB", state()->renderState().fogDetails()._colourAndDensity.a);
        pt.put("fog.fogDensityC", state()->renderState().fogDetails()._colourSunScatter.a);
        pt.put("fog.fogColour.<xmlattr>.r", state()->renderState().fogDetails()._colourAndDensity.r);
        pt.put("fog.fogColour.<xmlattr>.g", state()->renderState().fogDetails()._colourAndDensity.g);
        pt.put("fog.fogColour.<xmlattr>.b", state()->renderState().fogDetails()._colourAndDensity.b);

        pt.put("lod.lodThresholds.<xmlattr>.x", state()->renderState().lodThresholds().x);
        pt.put("lod.lodThresholds.<xmlattr>.y", state()->renderState().lodThresholds().y);
        pt.put("lod.lodThresholds.<xmlattr>.z", state()->renderState().lodThresholds().z);
        pt.put("lod.lodThresholds.<xmlattr>.w", state()->renderState().lodThresholds().w);

        pt.put("shadowing.<xmlattr>.lightBleedBias", state()->lightBleedBias());
        pt.put("shadowing.<xmlattr>.minShadowVariance", state()->minShadowVariance());

        pt.put("dayNight.<xmlattr>.enabled", dayNightCycleEnabled());
        pt.put("dayNight.timeOfDay.<xmlattr>.hour", _dayNightData._time._hour);
        pt.put("dayNight.timeOfDay.<xmlattr>.minute", _dayNightData._time._minutes);
        pt.put("dayNight.timeOfDay.<xmlattr>.timeFactor", _dayNightData._speedFactor);

        if (copyFile(scenePath.c_str(), (resourceName() + ".xml").c_str(), scenePath.c_str(), (resourceName() + ".xml.bak").c_str(), true) == FileError::NONE) {
            XML::writeXML(sceneDataFile.c_str(), pt);
        } else {
            if_constexpr (!Config::Build::IS_SHIPPING_BUILD) {
                DIVIDE_UNEXPECTED_CALL();
            }
        }
    }

    if (msgCallback) {
        msgCallback("Saving scene graph data ...");
    }
    sceneGraph()->saveToXML(assetsFile, msgCallback);

    //save music
    {
        if (msgCallback) {
            msgCallback("Saving music data ...");
        }

        ptree pt = {};
        ACKNOWLEDGE_UNUSED(pt); //ToDo: Save music data :)


        if (copyFile((sceneLocation + "/").c_str(), "musicPlaylist.xml", (sceneLocation + "/").c_str(), "musicPlaylist.xml.bak", true) == FileError::NONE) {
            XML::writeXML((sceneLocation + "/" + "musicPlaylist.xml.dev").c_str(), pt);
        }
    }

    Console::printfn(Locale::get(_ID("XML_SAVE_SCENE_END")), resourceName().c_str());

    if (finishCallback) {
        finishCallback(true);
    }

    return true;
}

bool Scene::loadXML(const Str256& name) {
    Configuration& config = _context.config();
    ParamHandler& par = _context.paramHandler();

    const ResourcePath scenePath = Paths::g_xmlDataLocation + Paths::g_scenesLocation;

    Console::printfn(Locale::get(_ID("XML_LOAD_SCENE")), name.c_str());
    const ResourcePath sceneLocation(scenePath + "/" + name.c_str());
    const ResourcePath sceneDataFile(sceneLocation + ".xml");

    // A scene does not necessarily need external data files
    // Data can be added in code for simple scenes
    if (!fileExists(sceneDataFile.c_str())) {
        sceneGraph()->loadFromXML("assets.xml");
        loadMusicPlaylist(sceneLocation.str(), "musicPlaylist.xml", this, config);
        return true;
    }

    boost::property_tree::ptree pt;
    XML::readXML(sceneDataFile.c_str(), pt);

    state()->renderState().grassVisibility(pt.get("vegetation.grassVisibility", 1000.0f));
    state()->renderState().treeVisibility(pt.get("vegetation.treeVisibility", 1000.0f));
    state()->renderState().generalVisibility(pt.get("options.visibility", 1000.0f));

    state()->windDirX(pt.get("wind.windDirX", 1.0f));
    state()->windDirZ(pt.get("wind.windDirZ", 1.0f));
    state()->windSpeed(pt.get("wind.windSpeed", 1.0f));

    state()->lightBleedBias(pt.get("shadowing.<xmlattr>.lightBleedBias", 0.2f));
    state()->minShadowVariance(pt.get("shadowing.<xmlattr>.minShadowVariance", 0.001f));

    dayNightCycleEnabled(pt.get("dayNight.<xmlattr>.enabled", false));
    _dayNightData._time._hour = pt.get<U8>("dayNight.timeOfDay.<xmlattr>.hour", 14u);
    _dayNightData._time._minutes = pt.get<U8>("dayNight.timeOfDay.<xmlattr>.minute", 30u);
    _dayNightData._speedFactor = pt.get("dayNight.timeOfDay.<xmlattr>.timeFactor", 1.0f);
    _dayNightData._resetTime = true;

    if (pt.get_child_optional("options.cameraStartPosition")) {
        par.setParam(_ID((name + ".options.cameraStartPosition.x").c_str()), pt.get("options.cameraStartPosition.<xmlattr>.x", 0.0f));
        par.setParam(_ID((name + ".options.cameraStartPosition.y").c_str()), pt.get("options.cameraStartPosition.<xmlattr>.y", 0.0f));
        par.setParam(_ID((name + ".options.cameraStartPosition.z").c_str()), pt.get("options.cameraStartPosition.<xmlattr>.z", 0.0f));
        par.setParam(_ID((name + ".options.cameraStartOrientation.xOffsetDegrees").c_str()), pt.get("options.cameraStartPosition.<xmlattr>.xOffsetDegrees", 0.0f));
        par.setParam(_ID((name + ".options.cameraStartOrientation.yOffsetDegrees").c_str()), pt.get("options.cameraStartPosition.<xmlattr>.yOffsetDegrees", 0.0f));
        par.setParam(_ID((name + ".options.cameraStartPositionOverride").c_str()), true);
    } else {
        par.setParam(_ID((name + ".options.cameraStartPositionOverride").c_str()), false);
    }

    if (pt.get_child_optional("options.autoCookPhysicsAssets")) {
        par.setParam(_ID((name + ".options.autoCookPhysicsAssets").c_str()), pt.get<bool>("options.autoCookPhysicsAssets", false));
    } else {
        par.setParam(_ID((name + ".options.autoCookPhysicsAssets").c_str()), false);
    }

    if (pt.get_child_optional("options.cameraSpeed")) {
        par.setParam(_ID((name + ".options.cameraSpeed.move").c_str()), pt.get("options.cameraSpeed.<xmlattr>.move", 35.0f));
        par.setParam(_ID((name + ".options.cameraSpeed.turn").c_str()), pt.get("options.cameraSpeed.<xmlattr>.turn", 35.0f));
    } else {
        par.setParam(_ID((name + ".options.cameraSpeed.move").c_str()), 35.0f);
        par.setParam(_ID((name + ".options.cameraSpeed.turn").c_str()), 35.0f);
    }

    FogDetails details = {};
    details._colourAndDensity.set(config.rendering.fogColour, config.rendering.fogDensity.x);
    details._colourSunScatter.a = config.rendering.fogDensity.y;

    if (pt.get_child_optional("fog")) {
        details._colourAndDensity = { 
            pt.get<F32>("fog.fogColour.<xmlattr>.r", details._colourAndDensity.r),
            pt.get<F32>("fog.fogColour.<xmlattr>.g", details._colourAndDensity.g),
            pt.get<F32>("fog.fogColour.<xmlattr>.b", details._colourAndDensity.b),
            pt.get("fog.fogDensityB", details._colourAndDensity.a)
        };
        details._colourSunScatter.a = pt.get("fog.fogDensityC", details._colourSunScatter.a);
    }
    

    state()->renderState().fogDetails(details);

    vec4<U16> lodThresholds(config.rendering.lodThresholds);

    if (pt.get_child_optional("lod")) {
        lodThresholds.set(pt.get<U16>("lod.lodThresholds.<xmlattr>.x", lodThresholds.x),
                          pt.get<U16>("lod.lodThresholds.<xmlattr>.y", lodThresholds.y),
                          pt.get<U16>("lod.lodThresholds.<xmlattr>.z", lodThresholds.z),
                          pt.get<U16>("lod.lodThresholds.<xmlattr>.w", lodThresholds.w));
    }

    state()->renderState().lodThresholds().set(lodThresholds);
    sceneGraph()->loadFromXML(pt.get("assets", "assets.xml").c_str());
    loadMusicPlaylist(sceneLocation.str(), pt.get("musicPlaylist", ""), this, config);

    return true;
}

SceneNode_ptr Scene::createNode(const SceneNodeType type, const ResourceDescriptor& descriptor) const {
    switch (type) {
        case SceneNodeType::TYPE_WATER:            return CreateResource<WaterPlane>(_resCache, descriptor);
        case SceneNodeType::TYPE_TRIGGER:          return CreateResource<Trigger>(_resCache, descriptor);
        case SceneNodeType::TYPE_PARTICLE_EMITTER: return CreateResource<ParticleEmitter>(_resCache, descriptor);
        case SceneNodeType::TYPE_INFINITEPLANE:    return CreateResource<InfinitePlane>(_resCache, descriptor);
        default: break;
    }

    // Warning?
    return nullptr;
}

void Scene::loadAsset(const Task* parentTask, const XML::SceneNode& sceneNode, SceneGraphNode* parent) {
    assert(parent != nullptr);

    const ResourcePath scenePath = Paths::g_xmlDataLocation + Paths::g_scenesLocation;
    const ResourcePath sceneLocation(scenePath + "/" + resourceName().c_str());
    const ResourcePath nodePath = sceneLocation + "/nodes/" + parent->name() + "_" + sceneNode.name + ".xml";

    SceneGraphNode* crtNode = parent;
    if (fileExists(nodePath)) {

        U32 normalMask = to_base(ComponentType::TRANSFORM) |
                         to_base(ComponentType::BOUNDS) |
                         to_base(ComponentType::NETWORKING);


        boost::property_tree::ptree nodeTree = {};
        XML::readXML(nodePath.str(), nodeTree);

        const auto loadModelComplete = [this, &nodeTree](CachedResource* res) {
            static_cast<SceneNode*>(res)->loadFromXML(nodeTree);
            _loadingTasks.fetch_sub(1);
        };

        const auto IsPrimitive = [](const U64 nameHash) {
            constexpr std::array<U64, 3> primitiveNames = {
                _ID("BOX_3D"),
                _ID("QUAD_3D"),
                _ID("SPHERE_3D")
            };

            for (const U64 it : primitiveNames) {
                if (nameHash == it) {
                    return true;
                }
            }

            return false;
        };

        const ResourcePath modelName { nodeTree.get("model", "") };

        SceneNode_ptr ret = nullptr;
        bool skipAdd = true;
        bool nodeStatic = true;

        if (IsPrimitive(sceneNode.typeHash)) {// Primitive types (only top level)
            normalMask |= to_base(ComponentType::RENDERING);

            if (!modelName.empty()) {
                _loadingTasks.fetch_add(1);

                ResourceDescriptor item(sceneNode.name);
                item.assetName(modelName);

                if (Util::CompareIgnoreCase(modelName, "BOX_3D")) {
                    ret = CreateResource<Box3D>(_resCache, item);
                } else if (Util::CompareIgnoreCase(modelName, "SPHERE_3D")) {
                    ret = CreateResource<Sphere3D>(_resCache, item);
                } else if (Util::CompareIgnoreCase(modelName, "QUAD_3D")) {
                    P32 quadMask;
                    quadMask.i = 0;
                    quadMask.b[0] = 1;
                    item.mask(quadMask);
                    ret = CreateResource<Quad3D>(_resCache, item);

                    Quad3D* quad = static_cast<Quad3D*>(ret.get());
                    quad->setCorner(Quad3D::CornerLocation::TOP_LEFT,     vec3<F32>(0, 1, 0));
                    quad->setCorner(Quad3D::CornerLocation::TOP_RIGHT,    vec3<F32>(1, 1, 0));
                    quad->setCorner(Quad3D::CornerLocation::BOTTOM_LEFT,  vec3<F32>(0, 0, 0));
                    quad->setCorner(Quad3D::CornerLocation::BOTTOM_RIGHT, vec3<F32>(1, 0, 0));
                } else {
                    Console::errorfn(Locale::get(_ID("ERROR_SCENE_UNSUPPORTED_GEOM")), sceneNode.name.c_str());
                }
            }
            if (ret != nullptr) {
                ResourceDescriptor materialDescriptor(sceneNode.name + "_material");
                Material_ptr tempMaterial = CreateResource<Material>(_resCache, materialDescriptor);
                tempMaterial->shadingMode(ShadingMode::BLINN_PHONG);
                ret->setMaterialTpl(tempMaterial);
                ret->addStateCallback(ResourceState::RES_LOADED, loadModelComplete);
            }
            skipAdd = false;
        } else {
            switch (sceneNode.typeHash) {
                case _ID("ROOT"): {
                    // Nothing to do with the root. This hasn't been used for a while
                } break;
                case _ID("TERRAIN"): {
                    _loadingTasks.fetch_add(1);
                    normalMask |= to_base(ComponentType::RENDERING);
                    addTerrain(parent, nodeTree, sceneNode.name);
                } break;
                case _ID("VEGETATION_GRASS"): {
                    normalMask |= to_base(ComponentType::RENDERING);
                    NOP(); //we rebuild grass every time
                } break;
                case _ID("INFINITE_PLANE"): {
                    _loadingTasks.fetch_add(1);
                    normalMask |= to_base(ComponentType::RENDERING);
                    addInfPlane(parent, nodeTree, sceneNode.name);
                } break;
                case _ID("WATER"): {
                    _loadingTasks.fetch_add(1);
                    normalMask |= to_base(ComponentType::RENDERING);
                    addWater(parent, nodeTree, sceneNode.name);
                } break;
                case _ID("MESH"): {
                    // No rendering component for meshes. Only for SubMeshes
                    //normalMask |= to_base(ComponentType::RENDERING);
                    if (!modelName.empty()) {
                        _loadingTasks.fetch_add(1);
                        ResourceDescriptor model(modelName.str());
                        model.assetLocation(Paths::g_assetsLocation + "models");
                        model.assetName(modelName);
                        model.flag(true);
                        model.threaded(false);
                        model.waitForReady(false);
                        Mesh_ptr meshPtr = CreateResource<Mesh>(_resCache, model);
                        if (meshPtr->getObjectFlag(Object3D::ObjectFlag::OBJECT_FLAG_SKINNED)) {
                            nodeStatic = false;
                        }
                        ret = meshPtr;
                        ret->addStateCallback(ResourceState::RES_LOADED, loadModelComplete);
                    }

                    skipAdd = false;
                } break;
                // SubMesh (change component properties, as the meshes should already be loaded)
                case _ID("SUBMESH"): {
                    while (parent->getNode().getState() != ResourceState::RES_LOADED) {
                        if (parentTask != nullptr) {
                            parentTask->_parentPool->threadWaiting();
                        }
                    }
                    normalMask |= to_base(ComponentType::RENDERING);
                    SceneGraphNode* subMesh = parent->findChild(_ID(sceneNode.name.c_str()), false, false);
                    if (subMesh != nullptr) {
                        subMesh->loadFromXML(nodeTree);
                    }
                } break;
                case _ID("SKY"): {
                    //ToDo: Change this - Currently, just load the default sky.
                    normalMask |= to_base(ComponentType::RENDERING);
                    addSky(parent, nodeTree, sceneNode.name);
                } break;
                // Everything else
                default:
                case _ID("TRANSFORM"): {
                    skipAdd = false;
                } break;
            }
        }

        if (!skipAdd) {
            SceneGraphNodeDescriptor nodeDescriptor = {};
            nodeDescriptor._name = sceneNode.name;
            nodeDescriptor._componentMask = normalMask;
            nodeDescriptor._node = ret;
            nodeDescriptor._usageContext = nodeStatic ? NodeUsageContext::NODE_STATIC : NodeUsageContext::NODE_DYNAMIC;

            for (U8 i = 1; i < to_U8(ComponentType::COUNT); ++i) {
                const ComponentType type = ComponentType::_from_integral(1u << i);
                if (nodeTree.count(type._to_string()) != 0) {
                    nodeDescriptor._componentMask |= 1 << i;
                }
            }

            crtNode = parent->addChildNode(nodeDescriptor);
            Attorney::SceneGraphNodeScene::reserveChildCount(crtNode, sceneNode.children.size());

            crtNode->loadFromXML(nodeTree);
        }
    }

    const U32 childCount = to_U32(sceneNode.children.size());
    if (childCount == 1u) {
        loadAsset(parentTask, sceneNode.children.front(), crtNode);
    } else if (childCount > 1u) {
        ParallelForDescriptor descriptor = {};
        descriptor._iterCount = childCount;
        descriptor._partitionSize = 3u;
        descriptor._priority = TaskPriority::DONT_CARE;
        descriptor._useCurrentThread = true;
        descriptor._cbk = [this, &sceneNode, &crtNode](const Task* innerTask, const U32 start, const U32 end) {
                                for (U32 i = start; i < end; ++i) {
                                    loadAsset(innerTask, sceneNode.children[i], crtNode);
                                }
                            };
        parallel_for(_context, descriptor);
    }
}

SceneGraphNode* Scene::addParticleEmitter(const Str64& name,
                                          std::shared_ptr<ParticleData> data,
                                          SceneGraphNode* parentNode) const
{
    DIVIDE_ASSERT(!name.empty(),
                  "Scene::addParticleEmitter error: invalid name specified!");

    const ResourceDescriptor particleEmitter(name);
    std::shared_ptr<ParticleEmitter> emitter = CreateResource<ParticleEmitter>(_resCache, particleEmitter);

    DIVIDE_ASSERT(emitter != nullptr,
                  "Scene::addParticleEmitter error: Could not instantiate emitter!");

    if (Runtime::isMainThread()) {
        if (!emitter->initData(data)) {
            DIVIDE_UNEXPECTED_CALL();
        }
    } else {
        _context.app().mainThreadTask([&emitter, &data] { 
            if (!emitter->initData(data)) {
                DIVIDE_UNEXPECTED_CALL();
            }
        });
    }

    SceneGraphNodeDescriptor particleNodeDescriptor;
    particleNodeDescriptor._node = emitter;
    particleNodeDescriptor._usageContext = NodeUsageContext::NODE_DYNAMIC;
    particleNodeDescriptor._componentMask = to_base(ComponentType::TRANSFORM) |
                                            to_base(ComponentType::BOUNDS) |
                                            to_base(ComponentType::RENDERING) |
                                            to_base(ComponentType::NETWORKING) |
                                            to_base(ComponentType::SELECTION);

    return parentNode->addChildNode(particleNodeDescriptor);
}

void Scene::addTerrain(SceneGraphNode* parentNode, const boost::property_tree::ptree& pt, const Str64& nodeName) {
    Console::printfn(Locale::get(_ID("XML_LOAD_TERRAIN")), nodeName.c_str());

    // Load the rest of the terrain
    std::shared_ptr<TerrainDescriptor> ter = std::make_shared<TerrainDescriptor>((nodeName + "_descriptor").c_str());
    if (!ter->loadFromXML(pt, nodeName.c_str())) {
        return;
    }

    const auto registerTerrain = [this, nodeName, &parentNode, pt](CachedResource* res) {
        SceneGraphNodeDescriptor terrainNodeDescriptor;
        terrainNodeDescriptor._name = nodeName;
        terrainNodeDescriptor._node = std::static_pointer_cast<Terrain>(res->shared_from_this());
        terrainNodeDescriptor._usageContext = NodeUsageContext::NODE_STATIC;
        terrainNodeDescriptor._componentMask = to_base(ComponentType::NAVIGATION) |
                                               to_base(ComponentType::TRANSFORM) |
                                               to_base(ComponentType::RIGID_BODY) |
                                               to_base(ComponentType::BOUNDS) |
                                               to_base(ComponentType::RENDERING) |
                                               to_base(ComponentType::NETWORKING);
        terrainNodeDescriptor._node->loadFromXML(pt);

        SceneGraphNode* terrainTemp = parentNode->addChildNode(terrainNodeDescriptor);

        NavigationComponent* nComp = terrainTemp->get<NavigationComponent>();
        nComp->navigationContext(NavigationComponent::NavigationContext::NODE_OBSTACLE);

        terrainTemp->get<RigidBodyComponent>()->physicsGroup(PhysicsGroup::GROUP_STATIC);
        terrainTemp->loadFromXML(pt);
        _loadingTasks.fetch_sub(1);
    };

    ResourceDescriptor descriptor(ter->getVariable("terrainName"));
    descriptor.propertyDescriptor(*ter);
    descriptor.threaded(false);
    descriptor.flag(ter->active());
    descriptor.waitForReady(false);
    Terrain_ptr ret = CreateResource<Terrain>(_resCache, descriptor);
    ret->addStateCallback(ResourceState::RES_LOADED, registerTerrain);
}

void Scene::toggleFlashlight(const PlayerIndex idx) {
    SceneGraphNode*& flashLight = _flashLight[idx];
    if (!flashLight) {
        SceneGraphNodeDescriptor lightNodeDescriptor;
        lightNodeDescriptor._serialize = false;
        lightNodeDescriptor._name = Util::StringFormat("Flashlight_%d", idx);
        lightNodeDescriptor._usageContext = NodeUsageContext::NODE_DYNAMIC;
        lightNodeDescriptor._componentMask = to_base(ComponentType::TRANSFORM) |
                                             to_base(ComponentType::BOUNDS) |
                                             to_base(ComponentType::NETWORKING) |
                                             to_base(ComponentType::SPOT_LIGHT);
        flashLight = _sceneGraph->getRoot()->addChildNode(lightNodeDescriptor);
        SpotLightComponent* spotLight = flashLight->get<SpotLightComponent>();
        spotLight->castsShadows(true);
        spotLight->setDiffuseColour(DefaultColours::WHITE.rgb);

        insert(_flashLight, idx, flashLight);

        _cameraUpdateListeners[idx] = playerCamera(idx)->addUpdateListener([this, idx](const Camera& cam) {
            if (idx < _scenePlayers.size() && idx < _flashLight.size() && _flashLight[idx]) {
                if (cam.getGUID() == _scenePlayers[getSceneIndexForPlayer(idx)]->camera()->getGUID()) {
                    TransformComponent* tComp = _flashLight[idx]->get<TransformComponent>();
                    tComp->setPosition(cam.getEye());
                    tComp->setRotationEuler(cam.getEuler());
                }
            }
        });
    }

    flashLight->get<SpotLightComponent>()->toggleEnabled();
}

SceneGraphNode* Scene::addSky(SceneGraphNode* parentNode, const boost::property_tree::ptree& pt, const Str64& nodeName) {
    ResourceDescriptor skyDescriptor("DefaultSky_"+ nodeName);
    skyDescriptor.ID(to_U32(std::floor(Camera::utilityCamera(Camera::UtilityCamera::DEFAULT)->getZPlanes().y * 2)));

    std::shared_ptr<Sky> skyItem = CreateResource<Sky>(_resCache, skyDescriptor);
    DIVIDE_ASSERT(skyItem != nullptr, "Scene::addSky error: Could not create sky resource!");
    skyItem->loadFromXML(pt);

    SceneGraphNodeDescriptor skyNodeDescriptor;
    skyNodeDescriptor._node = skyItem;
    skyNodeDescriptor._name = nodeName;
    skyNodeDescriptor._usageContext = NodeUsageContext::NODE_STATIC;
    skyNodeDescriptor._componentMask = to_base(ComponentType::TRANSFORM) |
                                       to_base(ComponentType::BOUNDS) |
                                       to_base(ComponentType::RENDERING) |
                                       to_base(ComponentType::NETWORKING);

    SceneGraphNode* skyNode = parentNode->addChildNode(skyNodeDescriptor);
    skyNode->setFlag(SceneGraphNode::Flags::VISIBILITY_LOCKED);
    skyNode->loadFromXML(pt);

    return skyNode;
}

void Scene::addWater(SceneGraphNode* parentNode, const boost::property_tree::ptree& pt, const Str64& nodeName) {
    auto registerWater = [this, nodeName, &parentNode, pt](CachedResource* res) {
        SceneGraphNodeDescriptor waterNodeDescriptor;
        waterNodeDescriptor._name = nodeName;
        waterNodeDescriptor._node = std::static_pointer_cast<WaterPlane>(res->shared_from_this());
        waterNodeDescriptor._usageContext = NodeUsageContext::NODE_STATIC;
        waterNodeDescriptor._componentMask = to_base(ComponentType::NAVIGATION) |
                                             to_base(ComponentType::TRANSFORM) |
                                             to_base(ComponentType::RIGID_BODY) |
                                             to_base(ComponentType::BOUNDS) |
                                             to_base(ComponentType::RENDERING) |
                                             to_base(ComponentType::NETWORKING);

        waterNodeDescriptor._node->loadFromXML(pt);

        SceneGraphNode* waterNode = parentNode->addChildNode(waterNodeDescriptor);
        waterNode->loadFromXML(pt);
        _loadingTasks.fetch_sub(1);
    };

    ResourceDescriptor waterDescriptor("Water_" + nodeName);
    waterDescriptor.waitForReady(false);
    WaterPlane_ptr ret = CreateResource<WaterPlane>(_resCache, waterDescriptor);
    ret->addStateCallback(ResourceState::RES_LOADED, registerWater);
}

SceneGraphNode* Scene::addInfPlane(SceneGraphNode* parentNode, const boost::property_tree::ptree& pt, const Str64& nodeName) {
    ResourceDescriptor planeDescriptor("InfPlane_" + nodeName);

    Camera* baseCamera = Camera::utilityCamera(Camera::UtilityCamera::DEFAULT);

    planeDescriptor.ID(to_U32(baseCamera->getZPlanes().max));

    auto planeItem = CreateResource<InfinitePlane>(_resCache, planeDescriptor);

    DIVIDE_ASSERT(planeItem != nullptr, "Scene::addInfPlane error: Could not create infinite plane resource!");
    planeItem->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource* res) noexcept {
        ACKNOWLEDGE_UNUSED(res);
        _loadingTasks.fetch_sub(1);
    });
    planeItem->loadFromXML(pt);

    SceneGraphNodeDescriptor planeNodeDescriptor;
    planeNodeDescriptor._node = planeItem;
    planeNodeDescriptor._name = nodeName;
    planeNodeDescriptor._usageContext = NodeUsageContext::NODE_STATIC;
    planeNodeDescriptor._componentMask = to_base(ComponentType::TRANSFORM) |
                                         to_base(ComponentType::BOUNDS) |
                                         to_base(ComponentType::RENDERING);

    SceneGraphNode* ret = parentNode->addChildNode(planeNodeDescriptor);
    ret->loadFromXML(pt);
    return ret;
}

U16 Scene::registerInputActions() {
    _input->flushCache();

    const auto none = [](const InputParams param) noexcept { ACKNOWLEDGE_UNUSED(param); };

    const auto deleteSelection = [this](const InputParams param) {
        const PlayerIndex idx = getPlayerIndexForDevice(param._deviceIndex);
        Selections& playerSelections = _currentSelection[idx];
        for (U8 i = 0u; i < playerSelections._selectionCount; ++i) {
            _sceneGraph->removeNode(playerSelections._selections[i]);
        }
        playerSelections._selectionCount = 0u;
        playerSelections._selections.fill(-1);
    };

    const auto increaseCameraSpeed = [this](const InputParams param){
        FreeFlyCamera* cam = _scenePlayers[getPlayerIndexForDevice(param._deviceIndex)]->camera();

        const F32 currentCamMoveSpeedFactor = cam->getMoveSpeedFactor();
        if (currentCamMoveSpeedFactor < 50) {
            cam->setMoveSpeedFactor(currentCamMoveSpeedFactor + 1.0f);
            cam->setTurnSpeedFactor(cam->getTurnSpeedFactor() + 1.0f);
        }
    };

    const auto decreaseCameraSpeed = [this](const InputParams param) {
        FreeFlyCamera* cam = _scenePlayers[getPlayerIndexForDevice(param._deviceIndex)]->camera();

        const F32 currentCamMoveSpeedFactor = cam->getMoveSpeedFactor();
        if (currentCamMoveSpeedFactor > 1.0f) {
            cam->setMoveSpeedFactor(currentCamMoveSpeedFactor - 1.0f);
            cam->setTurnSpeedFactor(cam->getTurnSpeedFactor() - 1.0f);
        }
    };

    const auto increaseResolution = [this](const InputParams param) { ACKNOWLEDGE_UNUSED(param); _context.gfx().increaseResolution(); };
    const auto decreaseResolution = [this](const InputParams param) { ACKNOWLEDGE_UNUSED(param); _context.gfx().decreaseResolution(); };

    const auto moveForward    = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveFB(MoveDirection::POSITIVE); };
    const auto moveBackwards  = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveFB(MoveDirection::NEGATIVE); };
    const auto stopMoveFWDBCK = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveFB(MoveDirection::NONE); };

    const auto strafeLeft          = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveLR(MoveDirection::NEGATIVE); };
    const auto strafeRight         = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveLR(MoveDirection::POSITIVE); };
    const auto stopStrafeLeftRight = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveLR(MoveDirection::NONE); };

    const auto rollCCW       = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).roll(MoveDirection::NEGATIVE); };
    const auto rollCW        = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).roll(MoveDirection::POSITIVE); };
    const auto stopRollCCWCW = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).roll(MoveDirection::NONE); };

    const auto turnLeft          = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).angleLR(MoveDirection::NEGATIVE); };
    const auto turnRight         = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).angleLR(MoveDirection::POSITIVE); };
    const auto stopTurnLeftRight = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).angleLR(MoveDirection::NONE); };

    const auto turnUp         = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).angleUD(MoveDirection::NEGATIVE); };
    const auto turnDown       = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).angleUD(MoveDirection::POSITIVE); };
    const auto stopTurnUpDown = [this](const InputParams param) { state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).angleUD(MoveDirection::NONE); };

    const auto togglePauseState = [this](const InputParams /*param*/){ _context.paramHandler().setParam(_ID("freezeLoopTime"), !_context.paramHandler().getParam(_ID("freezeLoopTime"), false)); };

    const auto takeScreenShot = [this](const InputParams param) { ACKNOWLEDGE_UNUSED(param); _context.gfx().screenshot("screenshot_"); };

    const auto toggleFullScreen = [this](const InputParams param) { ACKNOWLEDGE_UNUSED(param); _context.gfx().toggleFullScreen(); };

    const auto toggleFlashLight = [this](const InputParams param) { toggleFlashlight(getPlayerIndexForDevice(param._deviceIndex)); };

    const auto lockCameraToMouse      = [this](const InputParams  param) {
        if (!lockCameraToPlayerMouse(getPlayerIndexForDevice(param._deviceIndex), true)) {
            NOP();
        }
    };
    const auto releaseCameraFromMouse = [this](const InputParams  param) {
        if (!lockCameraToPlayerMouse(getPlayerIndexForDevice(param._deviceIndex), false)) {
            NOP();
        }
    };

    const auto shutdown = [this](const InputParams param) noexcept { ACKNOWLEDGE_UNUSED(param); _context.app().RequestShutdown();};

    const auto povNavigation = [this](const InputParams param) {
        const U32 povMask = param._var[0];

        if (povMask & to_base(Input::JoystickPovDirection::UP)) {  // Going up
            state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveFB(MoveDirection::POSITIVE);
        }
        if (povMask & to_base(Input::JoystickPovDirection::DOWN)) {  // Going down
            state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveFB(MoveDirection::NEGATIVE);
        }
        if (povMask & to_base(Input::JoystickPovDirection::RIGHT)) {  // Going right
            state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveLR(MoveDirection::POSITIVE);
        }
        if (povMask & to_base(Input::JoystickPovDirection::LEFT)) {  // Going left
            state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveLR(MoveDirection::NEGATIVE);
        }
        if (povMask == to_base(Input::JoystickPovDirection::CENTERED)) {  // stopped/centered out
            state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveLR(MoveDirection::NONE);
            state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveFB(MoveDirection::NONE);
        }
    };

    const auto axisNavigation = [this](const InputParams param) {
        const I32 axisABS = param._var[0];
        const I32 axis = param._var[1];
        //const bool isGamePad = param._var[2] == 1;
        const I32 deadZone = param._var[3];

        switch (axis) {
            case 0: {
                if (axisABS > deadZone) {
                    state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).angleUD(MoveDirection::POSITIVE);
                } else if (axisABS < -deadZone) {
                    state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).angleUD(MoveDirection::NEGATIVE);
                } else {
                    state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).angleUD(MoveDirection::NONE);
                }
            } break;
            case 1: {
                if (axisABS > deadZone) {
                    state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).angleLR(MoveDirection::POSITIVE);
                } else if (axisABS < -deadZone) {
                    state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).angleLR(MoveDirection::NEGATIVE);
                } else {
                    state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).angleLR(MoveDirection::NONE);
                }
            } break;

            case 2: {
                if (axisABS < -deadZone) {
                    state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveFB(MoveDirection::POSITIVE);
                } else if (axisABS > deadZone) {
                    state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveFB(MoveDirection::NEGATIVE);
                } else {
                    state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveFB(MoveDirection::NONE);
                }
            } break;
            case 3: {
                if (axisABS < -deadZone) {
                    state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveLR(MoveDirection::NEGATIVE);
                } else if (axisABS > deadZone) {
                    state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveLR(MoveDirection::POSITIVE);
                } else {
                    state()->playerState(getPlayerIndexForDevice(param._deviceIndex)).moveLR(MoveDirection::NONE);
                }
            } break;
            default: DIVIDE_UNEXPECTED_CALL(); break;
        }
    };

    const auto toggleDebugInterface = [this](const InputParams param) noexcept {
        ACKNOWLEDGE_UNUSED(param);

        _context.debug().toggle(!_context.debug().enabled());
    };

    const auto toggleEditor = [this](const InputParams param) {
        ACKNOWLEDGE_UNUSED(param);

        if_constexpr(Config::Build::ENABLE_EDITOR) {
            _context.editor().toggle(!_context.editor().running());
        }
    };

    const auto toggleConsole = [this](const InputParams param) {
        ACKNOWLEDGE_UNUSED(param);

        if_constexpr(Config::Build::ENABLE_EDITOR) {
            _context.gui().getConsole().setVisible(!_context.gui().getConsole().isVisible());
        }
    };

    const auto dragSelectBegin = [this](const InputParams param) { beginDragSelection(getPlayerIndexForDevice(param._deviceIndex), vec2<I32>(param._var[2], param._var[3])); };
    const auto dragSelectEnd   = [this](const InputParams param) { endDragSelection(getPlayerIndexForDevice(param._deviceIndex), true); };

    InputActionList& actions = _input->actionList();
    bool ret = true;
    U16 actionID = 0;
    ret = actions.registerInputAction(actionID++, none) && ret;                   // 0
    ret = actions.registerInputAction(actionID++, deleteSelection) && ret;        // 1
    ret = actions.registerInputAction(actionID++, increaseCameraSpeed) && ret;    // 2
    ret = actions.registerInputAction(actionID++, decreaseCameraSpeed) && ret;    // 3
    ret = actions.registerInputAction(actionID++, increaseResolution) && ret;     // 4
    ret = actions.registerInputAction(actionID++, decreaseResolution) && ret;     // 5
    ret = actions.registerInputAction(actionID++, moveForward) && ret;            // 6
    ret = actions.registerInputAction(actionID++, moveBackwards) && ret;          // 7
    ret = actions.registerInputAction(actionID++, stopMoveFWDBCK) && ret;         // 8
    ret = actions.registerInputAction(actionID++, strafeLeft) && ret;             // 9
    ret = actions.registerInputAction(actionID++, strafeRight) && ret;            // 10
    ret = actions.registerInputAction(actionID++, stopStrafeLeftRight) && ret;    // 11
    ret = actions.registerInputAction(actionID++, rollCCW) && ret;                // 12
    ret = actions.registerInputAction(actionID++, rollCW) && ret;                 // 13
    ret = actions.registerInputAction(actionID++, stopRollCCWCW) && ret;          // 14
    ret = actions.registerInputAction(actionID++, turnLeft) && ret;               // 15
    ret = actions.registerInputAction(actionID++, turnRight) && ret;              // 16
    ret = actions.registerInputAction(actionID++, stopTurnLeftRight) && ret;      // 17
    ret = actions.registerInputAction(actionID++, turnUp) && ret;                 // 18
    ret = actions.registerInputAction(actionID++, turnDown) && ret;               // 19
    ret = actions.registerInputAction(actionID++, stopTurnUpDown) && ret;         // 20
    ret = actions.registerInputAction(actionID++, togglePauseState) && ret;       // 21
    ret = actions.registerInputAction(actionID++, takeScreenShot) && ret;         // 22
    ret = actions.registerInputAction(actionID++, toggleFullScreen) && ret;       // 23
    ret = actions.registerInputAction(actionID++, toggleFlashLight) && ret;       // 24
    ret = actions.registerInputAction(actionID++, lockCameraToMouse) && ret;      // 25
    ret = actions.registerInputAction(actionID++, releaseCameraFromMouse) && ret; // 26
    ret = actions.registerInputAction(actionID++, shutdown) && ret;               // 27
    ret = actions.registerInputAction(actionID++, povNavigation) && ret;          // 28
    ret = actions.registerInputAction(actionID++, axisNavigation) && ret;         // 29
    ret = actions.registerInputAction(actionID++, toggleDebugInterface) && ret;   // 30
    ret = actions.registerInputAction(actionID++, toggleEditor) && ret;           // 31
    ret = actions.registerInputAction(actionID++, toggleConsole) && ret;          // 32
    ret = actions.registerInputAction(actionID++, dragSelectBegin) && ret;        // 33
    ret = actions.registerInputAction(actionID++, dragSelectEnd) && ret;          // 34

    if (!ret) {
        DIVIDE_UNEXPECTED_CALL();
    }

    return actionID;
}

void Scene::loadKeyBindings() {
    XML::loadDefaultKeyBindings((Paths::g_xmlDataLocation + "keyBindings.xml").str(), this);
}

bool Scene::lockCameraToPlayerMouse(const PlayerIndex index, const bool lockState) const
{
    static bool hadWindowGrab = false;
    static vec2<I32> lastMousePosition;

    state()->playerState(index).cameraLockedToMouse(lockState);

    DisplayWindow* window = _context.app().windowManager().getFocusedWindow();
    if (lockState) {
        if (window != nullptr) {
            hadWindowGrab = window->grabState();
        }
        lastMousePosition = WindowManager::GetGlobalCursorPosition();
        WindowManager::ToggleRelativeMouseMode(true);
    } else {
        WindowManager::ToggleRelativeMouseMode(false);
        state()->playerState(index).resetMovement();
        if (window != nullptr) {
            window->grabState(hadWindowGrab);
        }
        WindowManager::SetGlobalCursorPosition(lastMousePosition.x, lastMousePosition.y);
    }

    return true;
}

void Scene::loadDefaultCamera() {
    const ParamHandler& par = _context.paramHandler();
    
    // Camera position is overridden in the scene's XML configuration file
    if (!par.isParam<bool>(_ID((resourceName() + ".options.cameraStartPositionOverride").c_str()))) {
        return;
    }

    FreeFlyCamera* baseCamera = Camera::utilityCamera<FreeFlyCamera>(Camera::UtilityCamera::DEFAULT);
    if (par.getParam<bool>(_ID((resourceName() + ".options.cameraStartPositionOverride").c_str()))) {
        baseCamera->setEye(
            par.getParam<F32>(_ID((resourceName() + ".options.cameraStartPosition.x").c_str())),
            par.getParam<F32>(_ID((resourceName() + ".options.cameraStartPosition.y").c_str())),
            par.getParam<F32>(_ID((resourceName() + ".options.cameraStartPosition.z").c_str()))
        );

        baseCamera->setGlobalRotation(
            par.getParam<F32>(_ID((resourceName() + ".options.cameraStartOrientation.xOffsetDegrees").c_str())), //yaw
            par.getParam<F32>(_ID((resourceName() + ".options.cameraStartOrientation.yOffsetDegrees").c_str()))  //pitch
        );
    } else {
        baseCamera->setEye(0.f, 50.f, 0.f);
    }

    baseCamera->setMoveSpeedFactor(par.getParam<F32>(_ID((resourceName() + ".options.cameraSpeed.move").c_str()), 1.0f));
    baseCamera->setTurnSpeedFactor(par.getParam<F32>(_ID((resourceName() + ".options.cameraSpeed.turn").c_str()), 1.0f));

    const F32 hFoV = _context.config().runtime.horizontalFOV;
    const F32 vFoV = Angle::to_VerticalFoV(hFoV, to_D64(baseCamera->getAspectRatio()));
    baseCamera->setProjection(vFoV, { Camera::s_minNearZ, _context.config().runtime.cameraViewDistance });
}

bool Scene::load(const Str256& name) {
    setState(ResourceState::RES_LOADING);
    std::atomic_init(&_loadingTasks, 0u);

    _resourceName = name;

    loadDefaultCamera();

    SceneGraphNode* rootNode = _sceneGraph->getRoot();
    vectorEASTL<XML::SceneNode>& rootChildren = _xmlSceneGraphRootNode.children;
    const size_t childCount = rootChildren.size();
    Attorney::SceneGraphNodeScene::reserveChildCount(rootNode, childCount);

    ParallelForDescriptor descriptor = {};
    descriptor._iterCount = to_U32(childCount);
    descriptor._partitionSize = 3u;
    descriptor._priority = TaskPriority::DONT_CARE;
    descriptor._useCurrentThread = true;
    descriptor._allowPoolIdle = true;
    descriptor._waitForFinish = true;
    descriptor._cbk = [this, &rootNode, &rootChildren](const Task* parentTask, const U32 start, const U32 end) {
                            for (U32 i = start; i < end; ++i) {
                                loadAsset(parentTask, rootChildren[i], rootNode);
                            }
                        };
    parallel_for(_context, descriptor);

    WAIT_FOR_CONDITION(_loadingTasks.load() == 0u)

    // We always add a sky
    const auto& skies = sceneGraph()->getNodesByType(SceneNodeType::TYPE_SKY);
    assert(!skies.empty());
    Sky& currentSky = skies[0]->getNode<Sky>();
    const auto& dirLights = _lightPool->getLights(LightType::DIRECTIONAL);

    DirectionalLightComponent *sun = nullptr;
    for (auto light : dirLights) {
        const auto dirLight = light->getSGN()->get<DirectionalLightComponent>();
        if (dirLight->tag() == SUN_LIGHT_TAG) {
            sun = dirLight;
            break;
        }
    }
    if (sun != nullptr) {
        sun->castsShadows(true);
        initDayNightCycle(currentSky, *sun);
    }

    _loadComplete = true;

    return _loadComplete;
}

bool Scene::unload() {
    _aiManager->stop();
    WAIT_FOR_CONDITION(!_aiManager->running())

    U32 totalLoadingTasks = _loadingTasks.load();
    while (totalLoadingTasks > 0) {
        const U32 actualTasks = _loadingTasks.load();
        if (totalLoadingTasks != actualTasks) {
            totalLoadingTasks = actualTasks;
        }
        std::this_thread::yield();
    }

    clearTasks();
    if (_pxScene != nullptr) {
        /// Destroy physics (:D)
        _pxScene->release();
        MemoryManager::DELETE(_pxScene);
    }

    _context.pfx().setPhysicsScene(nullptr);
    clearObjects();
    _loadComplete = false;
    assert(_scenePlayers.empty());

    return true;
}

void Scene::postLoad() {
    _sceneGraph->postLoad();

    if (_pxScene == nullptr) {
        _pxScene = _context.pfx().NewSceneInterface(*this);
        _pxScene->init();
    }

    // Cook geometry
    if (_context.paramHandler().getParam<bool>(_ID((resourceName() + ".options.autoCookPhysicsAssets").c_str()), true)) {
        _cookCollisionMeshesScheduled = true;
    }
}

void Scene::postLoadMainThread(const Rect<U16>& targetRenderViewport) {
    assert(Runtime::isMainThread());
    ACKNOWLEDGE_UNUSED(targetRenderViewport);
    setState(ResourceState::RES_LOADED);
}

void Scene::rebuildShaders() {
    bool rebuilt = false;
    for (auto& [playerIdx, selections] : _currentSelection) {
        for (U8 i = 0u; i < selections._selectionCount; ++i) {
            SceneGraphNode* node = sceneGraph()->findNode(selections._selections[i]);
            if (node != nullptr) {
                node->get<RenderingComponent>()->rebuildMaterial();
                rebuilt = true;
            }
        }
    }

    if (!rebuilt) {
        ShaderProgram::RebuildAllShaders();
    }
}

stringImpl Scene::GetPlayerSGNName(const PlayerIndex idx) {
    return Util::StringFormat(g_defaultPlayerName, idx + 1);
}

void Scene::currentPlayerPass(const PlayerIndex idx) {
    //ToDo: These don't necessarily need to match -Ionut
    renderState().renderPass(idx);
    state()->playerPass(idx);

    if (state()->playerState().cameraUnderwater()) {
        _context.gfx().getRenderer().postFX().pushFilter(FilterType::FILTER_UNDERWATER);
    } else {
        _context.gfx().getRenderer().postFX().popFilter(FilterType::FILTER_UNDERWATER);
    }
}

void Scene::onSetActive() {
    _context.pfx().setPhysicsScene(_pxScene);
    _aiManager->pauseUpdate(false);

    input()->onSetActive();
    _context.sfx().stopMusic();
    _context.sfx().dumpPlaylists();

    for (U32 i = 0; i < to_base(MusicType::COUNT); ++i) {
        const SceneState::MusicPlaylist& playlist = state()->music(static_cast<MusicType>(i));
        if (!playlist.empty()) {
            for (const SceneState::MusicPlaylist::value_type& song : playlist) {
                _context.sfx().addMusic(i, song.second);
            }
        }
    }
    if (!_context.sfx().playMusic(0)) {
        //DIVIDE_UNEXPECTED_CALL();
        NOP();
    }

    assert(_parent.getActivePlayerCount() == 0);
    addPlayerInternal(false);

    _context.mainWindow().title("%s - %s", _context.mainWindow().title(), resourceName().c_str());
}

void Scene::onRemoveActive() {
    _aiManager->pauseUpdate(true);

    while(!_scenePlayers.empty()) {
        Attorney::SceneManagerScene::removePlayer(_parent, *this, _scenePlayers.back()->getBoundNode(), false);
    }

    input()->onRemoveActive();
}

void Scene::addPlayerInternal(const bool queue) {
    // Limit max player count
    if (_parent.getActivePlayerCount() == Config::MAX_LOCAL_PLAYER_COUNT) {
        return;
    }

    const stringImpl playerName = GetPlayerSGNName(static_cast<PlayerIndex>(_parent.getActivePlayerCount()));
    
    SceneGraphNode* playerSGN(_sceneGraph->findNode(playerName));
    if (!playerSGN) {
        SceneGraphNode* root = _sceneGraph->getRoot();

        SceneGraphNodeDescriptor playerNodeDescriptor;
        playerNodeDescriptor._serialize = false;
        playerNodeDescriptor._node = std::make_shared<SceneNode>(_resCache, to_size(generateGUID() + _parent.getActivePlayerCount()), playerName, ResourcePath{ playerName }, ResourcePath{}, SceneNodeType::TYPE_TRANSFORM, 0u);
        playerNodeDescriptor._name = playerName;
        playerNodeDescriptor._usageContext = NodeUsageContext::NODE_DYNAMIC;
        playerNodeDescriptor._componentMask = to_base(ComponentType::UNIT) |
                                              to_base(ComponentType::TRANSFORM) |
                                              to_base(ComponentType::BOUNDS) |
                                              to_base(ComponentType::NETWORKING);

        playerSGN = root->addChildNode(playerNodeDescriptor);
    }

    Attorney::SceneManagerScene::addPlayer(_parent, *this, playerSGN, queue);
    DIVIDE_ASSERT(playerSGN->get<UnitComponent>()->getUnit() != nullptr);
}

void Scene::removePlayerInternal(const PlayerIndex idx) {
    assert(idx < _scenePlayers.size());
    
    Attorney::SceneManagerScene::removePlayer(_parent, *this, _scenePlayers[getSceneIndexForPlayer(idx)]->getBoundNode(), true);
}

void Scene::onPlayerAdd(const Player_ptr& player) {
    _scenePlayers.push_back(player.get());
    state()->onPlayerAdd(player->index());
    input()->onPlayerAdd(player->index());
}

void Scene::onPlayerRemove(const Player_ptr& player) {
    const PlayerIndex idx = player->index();

    input()->onPlayerRemove(idx);
    state()->onPlayerRemove(idx);
    _cameraUpdateListeners[idx] = 0u;
    if (_flashLight.size() > idx) {
        SceneGraphNode* flashLight = _flashLight[idx];
        if (flashLight) {
            _sceneGraph->getRoot()->removeChildNode(flashLight);
            _flashLight[idx] = nullptr;
        }
    }
    _sceneGraph->getRoot()->removeChildNode(player->getBoundNode());

    _scenePlayers.erase(std::cbegin(_scenePlayers) + getSceneIndexForPlayer(idx));
}

U8 Scene::getSceneIndexForPlayer(const PlayerIndex idx) const {
    for (U8 i = 0; i < to_U8(_scenePlayers.size()); ++i) {
        if (_scenePlayers[i]->index() == idx) {
            return i;
        }
    }

    DIVIDE_UNEXPECTED_CALL();
    return 0;
}

Player* Scene::getPlayerForIndex(const PlayerIndex idx) const {
    return _scenePlayers[getSceneIndexForPlayer(idx)];
}

U8 Scene::getPlayerIndexForDevice(const U8 deviceIndex) const {
    return input()->getPlayerIndexForDevice(deviceIndex);
}

void Scene::clearObjects() {
    _xmlSceneGraphRootNode = {};
    _flashLight.clear();
    _sceneGraph->unload();
}

bool Scene::mouseMoved(const Input::MouseMoveEvent& arg) {
    if (!arg.wheelEvent()) {
        const PlayerIndex idx = getPlayerIndexForDevice(arg._deviceIndex);
        DragSelectData& data = _dragSelectData[idx];
        if (data._isDragging) {
            data._endDragPos = arg.absolutePos();
            updateSelectionData(idx, data, arg.remapped());
        } else {
            bool sceneFocused = true;
            bool sceneHovered = true;
            if_constexpr (Config::Build::ENABLE_EDITOR) {
                const Editor& editor = _context.editor();
                sceneFocused = !editor.running() || editor.scenePreviewFocused();
                sceneHovered = !editor.running() || editor.scenePreviewHovered();
            }

            if (sceneFocused && sceneHovered  && !state()->playerState(idx).cameraLockedToMouse()) {
                findHoverTarget(idx, arg.absolutePos());
            } else if (!sceneHovered) {
                clearHoverTarget(idx);
            }
        }
    }
    return false;
}

bool Scene::updateCameraControls(const PlayerIndex idx) const {
    FreeFlyCamera* cam = getPlayerForIndex(idx)->camera();
    
    SceneStatePerPlayer& playerState = state()->playerState(idx);

    playerState.previousViewMatrix(cam->viewMatrix());
    playerState.previousProjectionMatrix(cam->projectionMatrix());

    bool updated = false;
    updated = cam->moveRelative(vec3<I32>(to_I32(playerState.moveFB()),
                                          to_I32(playerState.moveLR()),
                                          to_I32(playerState.moveUD()))) || updated;

    updated = cam->rotateRelative(vec3<I32>(to_I32(playerState.angleUD()), //pitch
                                            to_I32(playerState.angleLR()), //yaw
                                            to_I32(playerState.roll()))) || updated; //roll
    updated = cam->zoom(to_I32(playerState.zoom())) || updated;

    playerState.cameraUpdated(updated);
    if (updated) {
        playerState.cameraUnderwater(checkCameraUnderwater(*cam));
        return true;
    }

    return false;
}

void Scene::updateSceneState(const U64 deltaTimeUS) {
    OPTICK_EVENT()

    _sceneTimerUS += deltaTimeUS;
    updateSceneStateInternal(deltaTimeUS);
    _sceneState->waterBodies().clear();
    _sceneGraph->sceneUpdate(deltaTimeUS, *_sceneState);
    _aiManager->update(deltaTimeUS);
}

void Scene::onStartUpdateLoop(const U8 loopNumber) {
    OPTICK_EVENT()

    _sceneGraph->onStartUpdateLoop(loopNumber);
}

void Scene::onLostFocus() {
    //Add a focus flag and ignore redundant calls

    for (const Player* player : _scenePlayers) {
        state()->playerState(player->index()).resetMovement();
        endDragSelection(player->index(), false);
    }
    _parent.wantsMouse(false);
    //_paramHandler.setParam(_ID("freezeLoopTime"), true);
}

void Scene::onGainFocus() {
    //Add a focus flag and ignore redundant calls
}

void Scene::registerTask(Task& taskItem, const bool start, const TaskPriority priority) {
    {
        UniqueLock<SharedMutex> w_lock(_tasksMutex);
        _tasks.push_back(&taskItem);
    }
    if (start) {
        Start(taskItem, priority);
    }
}

void Scene::clearTasks() {
    Console::printfn(Locale::get(_ID("STOP_SCENE_TASKS")));
    // Performance shouldn't be an issue here
    UniqueLock<SharedMutex> w_lock(_tasksMutex);
    for (Task* task : _tasks) {
        Wait(*task);
    }

    _tasks.clear();
}

void Scene::removeTask(Task& task) {
    UniqueLock<SharedMutex> w_lock(_tasksMutex);
    for (vectorEASTL<Task*>::iterator it = begin(_tasks); it != end(_tasks); ++it) {
        if ((*it)->_id == task._id) {
            Wait(**it);
            _tasks.erase(it);
            return;
        }
    }
}

void Scene::processInput(PlayerIndex idx, const U64 deltaTimeUS) {
    ACKNOWLEDGE_UNUSED(idx);
    ACKNOWLEDGE_UNUSED(deltaTimeUS);
}

void Scene::processGUI(const U64 deltaTimeUS) {
    const D64 delta = Time::MicrosecondsToMilliseconds<D64>(deltaTimeUS);

    eastl::transform(begin(_guiTimersMS), end(_guiTimersMS), begin(_guiTimersMS), [delta](const D64 timer) { return timer + delta; });
}

void Scene::processTasks(const U64 deltaTimeUS) {
    const D64 delta = Time::MicrosecondsToMilliseconds<D64>(deltaTimeUS);

    eastl::for_each(begin(_taskTimers), end(_taskTimers), [delta](D64& timer) { timer += delta; });

    if (_dayNightData._skyInstance != nullptr) {
        static struct tm timeOfDay = {};
        if (_dayNightData._resetTime) {
            time_t t = time(nullptr);
            timeOfDay = *localtime(&t);
            timeOfDay.tm_hour = _dayNightData._time._hour;
            timeOfDay.tm_min = _dayNightData._time._minutes;
            _dayNightData._resetTime = false;
            _dayNightData._timeAccumulatorSec = Time::Seconds(1.1f);
            _dayNightData._timeAccumulatorHour = 0.f;
            SceneEnvironmentProbePool::OnTimeOfDayChange(_envProbePool);
        }

        const F32 speedFactor = dayNightCycleEnabled() ? _dayNightData._speedFactor : 0.f;
        const F32 addTime = speedFactor * Time::MillisecondsToSeconds<F32>(delta);

        _dayNightData._timeAccumulatorSec += addTime;
        _dayNightData._timeAccumulatorHour += addTime;

        if (std::abs(_dayNightData._timeAccumulatorSec) > Time::Seconds(1.f)) {
            timeOfDay.tm_sec += to_I32(_dayNightData._timeAccumulatorSec);
            const time_t now = mktime(&timeOfDay); // normalize it
            _dayNightData._timeAccumulatorSec = 0.f;

            if (_dayNightData._timeAccumulatorHour > 1.f) {
                SceneEnvironmentProbePool::OnTimeOfDayChange(_envProbePool);
                _dayNightData._timeAccumulatorHour = 0.f;
            }
            FColour3 moonColour = Normalized(_dayNightData._skyInstance->moonColour().rgb);

            const SunDetails details = _dayNightData._skyInstance->setDateTime(localtime(&now));
            Light* light = _dayNightData._sunLight;

            const FColour3 sunsetOrange = FColour3(99.2f, 36.9f, 32.5f) / 100.f;

            // Sunset / sunrise
            vec3<Angle::DEGREES<F32>> eulerDirection = details._eulerDirection;
            light->getSGN()->get<TransformComponent>()->setRotationEuler(eulerDirection);
            // Morning
            if (IS_IN_RANGE_INCLUSIVE(details._intensity, 0.0f, 25.0f)) {
                light->setDiffuseColour(Lerp(sunsetOrange, DefaultColours::WHITE.rgb, details._intensity / 25.f));
            }
            // Early night time
            else if (IS_IN_RANGE_INCLUSIVE(details._intensity, -25.0f, 0.0f)) {
                light->setDiffuseColour(Lerp(sunsetOrange, moonColour, -details._intensity / 25.f));
            }
            // Night
            else if (details._intensity < -25.f) {
                light->setDiffuseColour(moonColour);
                light->getSGN()->get<TransformComponent>()->rotateZ(-180.f);
                light->getSGN()->get<TransformComponent>()->rotateX(-90.f);
            }
            // Day
            else {
                light->setDiffuseColour(DefaultColours::WHITE.rgb);
            }
            _dayNightData._time._hour = to_U8(timeOfDay.tm_hour);
            _dayNightData._time._minutes = to_U8(timeOfDay.tm_min);
        }
    }
}

void Scene::drawCustomUI(const Rect<I32>& targetViewport, GFX::CommandBuffer& bufferInOut) {
    ACKNOWLEDGE_UNUSED(targetViewport);

    if (_linesPrimitive->hasBatch()) {
        bufferInOut.add(_linesPrimitive->toCommandBuffer());
    }
}

void Scene::debugDraw(const Camera* activeCamera, const RenderStagePass stagePass, GFX::CommandBuffer& bufferInOut) {
    if_constexpr (!Config::Build::IS_SHIPPING_BUILD) {
        if (renderState().isEnabledOption(SceneRenderState::RenderOptions::RENDER_OCTREE_REGIONS)) {
            _octreeBoundingBoxes.resize(0);
            sceneGraph()->getOctree().getAllRegions(_octreeBoundingBoxes);

            const size_t primitiveCount = _octreePrimitives.size();
            const size_t regionCount = _octreeBoundingBoxes.size();
            if (regionCount > primitiveCount) {
                PipelineDescriptor pipeDesc;
                pipeDesc._stateHash = RenderStateBlock::defaultHash();
                pipeDesc._shaderProgramHandle = ShaderProgram::DefaultShader()->getGUID();
                Pipeline* pipeline = _context.gfx().newPipeline(pipeDesc);

                const size_t diff = regionCount - primitiveCount;
                for (size_t i = 0; i < diff; ++i) {
                    _octreePrimitives.push_back(_context.gfx().newIMP());
                    _octreePrimitives.back()->pipeline(*pipeline);
                }
            }

            assert(_octreePrimitives.size() >= _octreeBoundingBoxes.size());

            for (size_t i = 0; i < regionCount; ++i) {
                const BoundingBox& box = _octreeBoundingBoxes[i];
                _octreePrimitives[i]->fromBox(box.getMin(), box.getMax(), UColour4(255, 0, 255, 255));
                bufferInOut.add(_octreePrimitives[i]->toCommandBuffer());
            }
        }
    }

    // Show NavMeshes
    _aiManager->debugDraw(bufferInOut, false);
    _lightPool->drawLightImpostors(stagePass._stage, bufferInOut);
}

bool Scene::checkCameraUnderwater(const PlayerIndex idx) const {
    const Camera* crtCamera = Attorney::SceneManagerCameraAccessor::playerCamera(_parent, idx);
    return checkCameraUnderwater(*crtCamera);
}

bool Scene::checkCameraUnderwater(const Camera& camera) const {
    const vec3<F32>& eyePos = camera.getEye();
    {
        const auto& waterBodies = state()->waterBodies();
        for (const WaterBodyData& water : waterBodies) {
            const vec3<F32>& extents = water._extents;
            const vec3<F32>& position = water._positionW;
            const F32 halfWidth = (extents.x + position.x) * 0.5f;
            const F32 halfLength = (extents.z + position.z) * 0.5f;
            if (eyePos.x >= -halfWidth && eyePos.x <= halfWidth &&
                eyePos.z >= -halfLength && eyePos.z <= halfLength) {
                const float depth = -extents.y + position.y;
                return eyePos.y < position.y && eyePos.y > depth;
            }
        }
    }

    return false;
}

void Scene::findHoverTarget(PlayerIndex idx, const vec2<I32>& aimPos) {
    constexpr std::array<SceneNodeType, 6> s_ignoredNodes = {
     SceneNodeType::TYPE_TRANSFORM,
     SceneNodeType::TYPE_WATER,
     SceneNodeType::TYPE_SKY,
     SceneNodeType::TYPE_PARTICLE_EMITTER,
     SceneNodeType::TYPE_INFINITEPLANE,
     SceneNodeType::TYPE_VEGETATION
    };

    const Camera* crtCamera = getPlayerForIndex(idx)->camera();

    const vec2<F32>& zPlanes = crtCamera->getZPlanes();
    const Rect<I32>& viewport = _context.gfx().getCurrentViewport();

    const vec3<F32> startRay = crtCamera->getEye();
    const vec3<F32> direction = crtCamera->unProject(to_F32(aimPos.x), viewport.w - to_F32(aimPos.y), viewport);

    // see if we select another one
    _sceneSelectionCandidates.resize(0);

    SGNIntersectionParams intersectionParams = {};
    intersectionParams._ray = { startRay, direction };
    intersectionParams._range = zPlanes;
    intersectionParams._includeTransformNodes = true;
    intersectionParams._ignoredTypes = s_ignoredNodes.data();
    intersectionParams._ignoredTypesCount = s_ignoredNodes.size();

    // Cast the picking ray and find items between the nearPlane and far Plane
    sceneGraph()->intersect(intersectionParams, _sceneSelectionCandidates);

    if (!_sceneSelectionCandidates.empty()) {

        const auto IsValidTarget = [](SceneGraphNode* sgn, const bool inEditMode) {
            if (sgn == nullptr) {
                return false;
            }

            if (!inEditMode) {
                return sgn->get<SelectionComponent>() != nullptr &&
                    sgn->get<SelectionComponent>()->enabled();
            }

            SceneNodeType sType = sgn->getNode().type();
            if (sType == SceneNodeType::TYPE_OBJECT3D) {
                const ObjectType oType = sgn->getNode<Object3D>().getObjectType();

                if (!inEditMode && oType._value == ObjectType::SUBMESH ||
                    oType._value == ObjectType::TERRAIN) {
                    sType = SceneNodeType::COUNT;
                } else if (sgn->parent() != nullptr) {
                    sType = sgn->parent()->getNode().type();
                    if (sType == SceneNodeType::TYPE_TRANSFORM) {
                        sType = SceneNodeType::TYPE_OBJECT3D;
                    }
                }
            }

            return sType == SceneNodeType::TYPE_OBJECT3D ||
                sType == SceneNodeType::TYPE_TRIGGER ||
                sType == SceneNodeType::TYPE_PARTICLE_EMITTER;
        };

        // If we don't force selections, remove all of the nodes that lack a SelectionComponent
        eastl::sort(begin(_sceneSelectionCandidates),
                    end(_sceneSelectionCandidates),
                    [](const SGNRayResult& A, const SGNRayResult& B) noexcept -> bool {
                        return A.dist < B.dist;
                    });

        SceneGraphNode* target = nullptr;
        for (const SGNRayResult& result : _sceneSelectionCandidates) {
            if (result.dist < 0.0f) {
                continue;
            }

            SceneGraphNode* crtNode = _sceneGraph->findNode(result.sgnGUID);
            if (IsValidTarget(crtNode, (Config::Build::ENABLE_EDITOR ? _context.editor().inEditMode() : false))) {
                target = crtNode;
                break;
            }
        }

        clearHoverTarget(idx);

        if (target != nullptr) {
            _currentHoverTarget[idx] = target->getGUID();
            if (!target->hasFlag(SceneGraphNode::Flags::SELECTED)) {
                target->setFlag(SceneGraphNode::Flags::HOVERED, true);
            }
        }
    } else {
        clearHoverTarget(idx);
    }
}

void Scene::clearHoverTarget(const PlayerIndex idx) {

    if (_currentHoverTarget[idx] != -1) {
        SceneGraphNode* oldTarget = _sceneGraph->findNode(_currentHoverTarget[idx]);
        if (oldTarget != nullptr) {
            oldTarget->clearFlag(SceneGraphNode::Flags::HOVERED);
        }
    }

    _currentHoverTarget[idx] = -1;
}

void Scene::onNodeDestroy(SceneGraphNode* node) {
    const I64 guid = node->getGUID();
    for (auto& iter : _currentHoverTarget) {
        if (iter.second == guid) {
            iter.second = -1;
        }
    }

    for (auto& [playerIdx, playerSelections] : _currentSelection) {
        for (I8 i = playerSelections._selectionCount; i > 0; --i) {
            const I64 crtGUID = playerSelections._selections[i - 1];
            if (crtGUID == guid) {
                playerSelections._selections[i - 1] = -1;
                std::swap(playerSelections._selections[i - 1], playerSelections._selections[playerSelections._selectionCount--]);
            }
        }
    }

    _parent.onNodeDestroy(node);
}

void Scene::resetSelection(const PlayerIndex idx) {
    Selections& playerSelections = _currentSelection[idx];
    const U8 selectionCount = playerSelections._selectionCount;

    for (U8 i = 0; i < selectionCount; ++i) {
        SceneGraphNode* node = sceneGraph()->findNode(playerSelections._selections[i]);
        if (node != nullptr) {
            node->clearFlag(SceneGraphNode::Flags::HOVERED, true);
            node->clearFlag(SceneGraphNode::Flags::SELECTED, true);
        }
    }

    playerSelections.reset();
}

void Scene::setSelected(const PlayerIndex idx, const vectorEASTL<SceneGraphNode*>& SGNs, const bool recursive) {
    Selections& playerSelections = _currentSelection[idx];

    for (SceneGraphNode* sgn : SGNs) {
        if (!sgn->hasFlag(SceneGraphNode::Flags::SELECTED)) {
            playerSelections._selections[playerSelections._selectionCount++] = sgn->getGUID();
            sgn->setFlag(SceneGraphNode::Flags::SELECTED, recursive);
        }
    }
}

bool Scene::findSelection(const PlayerIndex idx, const bool clearOld) {
    // Clear old selection
    if (clearOld) {
        _parent.resetSelection(idx);
    }

    const I64 hoverGUID = _currentHoverTarget[idx];
    // No hover target
    if (hoverGUID == -1) {
        return false;
    }

    Selections& playerSelections = _currentSelection[idx];
    for (U8 i = 0; i < playerSelections._selectionCount; ++i) {
        if (playerSelections._selections[i] == hoverGUID) {
            //Already selected
            return true;
        }
    }

    SceneGraphNode* selectedNode = _sceneGraph->findNode(hoverGUID);
    if (selectedNode != nullptr) {
        _parent.setSelected(idx, { selectedNode }, false);
        return true;
    }
    _parent.resetSelection(idx);
    return false;
}

void Scene::beginDragSelection(const PlayerIndex idx, const vec2<I32>& mousePos) {
    const bool editorRunning = Config::Build::ENABLE_EDITOR ? _context.editor().running() : false;

    if_constexpr(Config::Build::ENABLE_EDITOR) {
        const Editor& editor = _context.editor();
        if (editor.running() && (!editor.scenePreviewFocused() || !editor.scenePreviewHovered())) {
            return;
        }
    }

    DragSelectData& data = _dragSelectData[idx];
    data._sourceViewport = editorRunning ? _context.gfx().getCurrentViewport() : _context.mainWindow().renderingViewport();
    if (data._sourceViewport.contains(mousePos)) {
        const vec2<U16>& resolution = _context.gfx().renderingResolution();
        data._targetViewport.set(0, 0, resolution.width, resolution.height);
        data._startDragPos = COORD_REMAP(mousePos, data._sourceViewport, data._targetViewport);
        data._endDragPos = data._startDragPos;
        data._isDragging = true;
    } else {
        data._isDragging = false;
    }
}

void Scene::updateSelectionData(PlayerIndex idx, DragSelectData& data, bool remapped) {
    static std::array<Line, 4> s_lines = {
        Line{VECTOR3_ZERO, VECTOR3_UNIT, DefaultColours::GREEN_U8, DefaultColours::GREEN_U8, 2.0f, 1.0f},
        Line{VECTOR3_ZERO, VECTOR3_UNIT, DefaultColours::GREEN_U8, DefaultColours::GREEN_U8, 2.0f, 1.0f},
        Line{VECTOR3_ZERO, VECTOR3_UNIT, DefaultColours::GREEN_U8, DefaultColours::GREEN_U8, 2.0f, 1.0f},
        Line{VECTOR3_ZERO, VECTOR3_UNIT, DefaultColours::GREEN_U8, DefaultColours::GREEN_U8, 2.0f, 1.0f}
    };

    if_constexpr(Config::Build::ENABLE_EDITOR) {
        const Editor& editor = _context.editor();
        if (editor.running()) {
            if (!editor.scenePreviewFocused()) {
                endDragSelection(idx, false);
                return;
            }
            if (!remapped) {
                const Rect<I32> previewRect = _context.editor().scenePreviewRect(false);
                data._endDragPos = COORD_REMAP(previewRect.clamp(data._endDragPos), previewRect, _context.gfx().getCurrentViewport());
            }
        }
    }

    _parent.wantsMouse(true);
    data._endDragPos = COORD_REMAP(data._endDragPos, data._sourceViewport, data._targetViewport);

    const vec2<I32> startPos = {
        data._startDragPos.x,
        data._targetViewport.w - data._startDragPos.y - 1
    };

    const vec2<I32> endPos = {
        data._endDragPos.x,
        data._targetViewport.w - data._endDragPos.y - 1
    };

    const I32 startX = std::min(startPos.x, endPos.x);
    const I32 startY = std::min(startPos.y, endPos.y);
    
    const Rect<I32> selectionRect = {
        startX,
        startY,
        std::abs(endPos.x - startPos.x),
        std::abs(endPos.y - startPos.y)
    };

    //X0, Y0 -> X1, Y0
    s_lines[0].positionStart({ selectionRect.x, selectionRect.y, 0 });
    s_lines[0].positionEnd({ selectionRect.x + selectionRect.z, selectionRect.y, 0 });
    
    //X1 , Y0 -> X1, Y1
    s_lines[1].positionStart({ selectionRect.x + selectionRect.z, selectionRect.y, 0 });
    s_lines[1].positionEnd({ selectionRect.x + selectionRect.z, selectionRect.y + selectionRect.w, 0 });
    
    //X1, Y1 -> X0, Y1
    s_lines[2].positionStart(s_lines[1].positionEnd());
    s_lines[2].positionEnd({ selectionRect.x, selectionRect.y + selectionRect.w, 0 });
    
    //X0, Y1 -> X0, Y0
    s_lines[3].positionStart(s_lines[2].positionEnd());
    s_lines[3].positionEnd(s_lines[0].positionStart());

    _linesPrimitive->fromLines(s_lines.data(), s_lines.size());

    if (_context.gfx().frameCount() % 2 == 0) {
        clearHoverTarget(idx);
        _parent.resetSelection(idx);
        Camera* crtCamera = getPlayerForIndex(idx)->camera();
        vectorEASTL<SceneGraphNode*> nodes = Attorney::SceneManagerScene::getNodesInScreenRect(_parent, selectionRect, *crtCamera, data._targetViewport);
        _parent.setSelected(idx, nodes, false);
    }
}

void Scene::endDragSelection(const PlayerIndex idx, const bool clearSelection) {
    constexpr F32 DRAG_SELECTION_THRESHOLD_PX_SQ = 9.f;

    auto& data = _dragSelectData[idx];

    _linesPrimitive->clearBatch();
    _parent.wantsMouse(false);
    data._isDragging = false;
    if (data._startDragPos.distanceSquared(data._endDragPos) < DRAG_SELECTION_THRESHOLD_PX_SQ) {
        findSelection(idx, clearSelection);
    }
}

void Scene::initDayNightCycle(Sky& skyInstance, DirectionalLightComponent& sunLight) noexcept {
    _dayNightData._skyInstance = &skyInstance;
    _dayNightData._sunLight = &sunLight;
    if (!_dayNightData._resetTime) {
        // Usually loaded from XML/save data
        _dayNightData._time = skyInstance.GetTimeOfDay();
        _dayNightData._resetTime = true;
    }
    _dayNightData._timeAccumulatorSec = Time::Seconds(1.1f);
    _dayNightData._timeAccumulatorHour = 0.f;
}

void Scene::setDayNightCycleTimeFactor(const F32 factor) noexcept {
    _dayNightData._speedFactor = factor;
}

F32 Scene::getDayNightCycleTimeFactor() const noexcept {
    return _dayNightData._speedFactor;
}

void Scene::setTimeOfDay(const SimpleTime& time) noexcept {
    _dayNightData._time = time;
    _dayNightData._resetTime = true;
}

const SimpleTime& Scene::getTimeOfDay() const noexcept {
    return _dayNightData._time;
}

SunDetails Scene::getCurrentSunDetails() const noexcept {
    if (_dayNightData._skyInstance != nullptr) {
        return _dayNightData._skyInstance->getCurrentDetails();
    }

    return {};
}

Sky::Atmosphere Scene::getCurrentAtmosphere() const noexcept {
    if (_dayNightData._skyInstance != nullptr) {
        return _dayNightData._skyInstance->atmosphere();
    }

    return {};
}

void Scene::setCurrentAtmosphere(const Sky::Atmosphere& atmosphere) const noexcept {
    if (_dayNightData._skyInstance != nullptr) {
        return _dayNightData._skyInstance->setAtmosphere(atmosphere);
    }
}

bool Scene::save(ByteBuffer& outputBuffer) const {
    const U8 playerCount = to_U8(_scenePlayers.size());
    outputBuffer << playerCount;
    for (U8 i = 0; i < playerCount; ++i) {
        const Camera* cam = _scenePlayers[i]->camera();
        outputBuffer << _scenePlayers[i]->index() << cam->getEye() << cam->getOrientation();
    }

    return _sceneGraph->saveCache(outputBuffer);
}

bool Scene::load(ByteBuffer& inputBuffer) {

    if (!inputBuffer.empty()) {
        const U8 currentPlayerCount = to_U8(_scenePlayers.size());

        vec3<F32> camPos;
        Quaternion<F32> camOrientation;

        U8 currentPlayerIndex = 0;
        U8 previousPlayerCount = 0;
        inputBuffer >> previousPlayerCount;
        for (U8 i = 0; i < previousPlayerCount; ++i) {
            inputBuffer >> currentPlayerIndex >> camPos >> camOrientation;
            if (currentPlayerIndex < currentPlayerCount) {
                Camera* cam = _scenePlayers[currentPlayerIndex]->camera();
                cam->setEye(camPos);
                cam->setRotation(camOrientation);
            }
        }
    }

    return _sceneGraph->loadCache(inputBuffer);
}

SceneShaderData* Scene::shaderData() const noexcept {
    return _parent.sceneData();
}

Camera* Scene::playerCamera() const {
    return Attorney::SceneManagerCameraAccessor::playerCamera(_parent);
}

Camera* Scene::playerCamera(const U8 index) const {
    return Attorney::SceneManagerCameraAccessor::playerCamera(_parent, index);
}

}