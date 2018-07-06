/*
   Copyright (c) 2016 DIVIDE-Studio
   Copyright (c) 2009 Ionut Cava

   This file is part of DIVIDE Framework.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software
   and associated documentation files (the "Software"), to deal in the Software
   without restriction,
   including without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
   PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
   IN CONNECTION WITH THE SOFTWARE
   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#ifndef _SCENE_H_
#define _SCENE_H_

#include "SceneState.h"
#include "SceneInput.h"
#include "Core/Headers/cdigginsAny.h"
#include "Platform/Threading/Headers/Task.h"

/*All these includes are useful for a scene, so instead of forward declaring the
  classes, we include the headers
  to make them available in every scene source file. To reduce compile times,
  forward declare the "Scene" class instead
*/
// Core files
#include "Core/Headers/Kernel.h"
#include "Core/Headers/Application.h"
#include "Graphs/Headers/SceneGraph.h"
#include "Rendering/Camera/Headers/Camera.h"
#include "Rendering/Lighting/Headers/LightPool.h"
// Hardware
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Input/Headers/InputInterface.h"
// Scene Elements
#include "SceneEnvironmentProbePool.h"
#include "AI/Headers/AIManager.h"
#include "Environment/Sky/Headers/Sky.h"
#include "Rendering/Lighting/Headers/Light.h"
#include "Rendering/Lighting/Headers/DirectionalLight.h"
#include "Physics/Headers/PXDevice.h"
#include "Dynamics/Entities/Particles/Headers/ParticleEmitter.h"
// GUI
#include "GUI/Headers/GUI.h"
#include "GUI/Headers/SceneGUIElements.h"

namespace Divide {
class Sky;
class Light;
class Object3D;
class LoadSave;
class TerrainDescriptor;
class ParticleEmitter;
class PhysicsSceneInterface;

namespace Attorney {
    class SceneManager;
    class SceneGraph;
    class SceneRenderPass;
    class SceneLoadSave;
    class SceneGUI;
};

/// The scene is a resource (to enforce load/unload and setName) and it has a 2 states:
/// one for game information and one for rendering information

class Scene : public Resource {
    friend class Attorney::SceneManager;
    friend class Attorney::SceneGraph;
    friend class Attorney::SceneRenderPass;
    friend class Attorney::SceneLoadSave;
    friend class Attorney::SceneGUI;

   protected:
    typedef std::stack<FileData, vectorImpl<FileData> > FileDataStack;
    static bool onStartup();
    static bool onShutdown();

   public:
    explicit Scene(const stringImpl& name);
    virtual ~Scene();

    /**Begin scene logic loop*/
    /// Get all input commands from the user
    virtual void processInput(const U64 deltaTime);
    /// Update the scene based on the inputs
    virtual void processTasks(const U64 deltaTime);
    virtual void processGUI(const U64 deltaTime);
    /// Scene is rendering, so add intensive tasks here to save CPU cycles
    bool idle();  
    /// The application has lost focus
    void onLostFocus();  
    /**End scene logic loop*/

    /// Update animations, network data, sounds, triggers etc.
    void updateSceneState(const U64 deltaTime);
    /// Override this for Scene specific updates
    virtual void updateSceneStateInternal(const U64 deltaTime) {}
    inline SceneState& state() { return *_sceneState; }
    inline const SceneState& state() const { return *_sceneState; }
    inline SceneRenderState& renderState() { return _sceneState->renderState(); }
    inline const SceneRenderState& renderState() const { return _sceneState->renderState(); }
    inline SceneInput& input() { return *_input; }

    inline SceneGraph& sceneGraph() { return *_sceneGraph; }
    void registerTask(const TaskHandle& taskItem);
    void clearTasks();
    void removeTask(I64 jobIdentifier);

    inline void addModel(FileData& model) { _modelDataArray.push(model); }
    inline void addTerrain(TerrainDescriptor* ter) {
        _terrainInfoArray.push_back(ter);
    }
    void addMusic(MusicType type, const stringImpl& name, const stringImpl& srcFile);
    void addPatch(vectorImpl<FileData>& data);

    // DIRECTIONAL lights have shadow mapping enabled automatically
    SceneGraphNode_ptr addLight(LightType type, SceneGraphNode& parentNode);
    SceneGraphNode_ptr addSky(const stringImpl& nodeName = "");

    /// Object picking
    inline SceneGraphNode_wptr getCurrentSelection() const {
        return _currentSelection;
    }
    inline SceneGraphNode_wptr getCurrentHoverTarget() const {
        return  _currentHoverTarget;
    }
    void findSelection();

    inline void addSelectionCallback(const DELEGATE_CBK<>& selectionCallback) {
        _selectionChangeCallbacks.push_back(selectionCallback);
    }

    SceneGraphNode_ptr addParticleEmitter(const stringImpl& name,
                                          std::shared_ptr<ParticleData> data,
                                          SceneGraphNode& parentNode);

    inline vectorImpl<FileData>& getVegetationDataArray() {
        return _vegetationDataArray;
    }


    inline AI::AIManager& aiManager() { return *_aiManager; }
    inline const AI::AIManager& aiManager() const { return *_aiManager; }

   protected:
    virtual void rebuildShaders();
    virtual void onSetActive();
    virtual void onRemoveActive();
    // returns the first available action ID
    virtual U16 registerInputActions();
    virtual void loadKeyBindings();

    void resetSelection();
    void findHoverTarget();
    void toggleFlashlight();

    virtual bool save(ByteBuffer& outputBuffer) const;
    virtual bool load(ByteBuffer& inputBuffer);

    virtual bool frameStarted();
    virtual bool frameEnded();
    /// Description in SceneManager
    virtual bool loadResources(bool continueOnErrors);
    virtual bool loadTasks(bool continueOnErrors) { return true; }
    virtual bool loadPhysics(bool continueOnErrors);
    /// if singleStep is true, only the first model from the modelArray will be
    /// loaded.
    /// Useful for loading one model per frame
    virtual void loadXMLAssets(bool singleStep = false);
    virtual bool load(const stringImpl& name);
    bool loadModel(const FileData& data);
    bool loadGeometry(const FileData& data);
    virtual bool unload();
    virtual void postLoad();
    // gets called on the main thread when the scene finishes loading
    // used by the GUI system
    virtual void postLoadMainThread();
    /// Description in SceneManager
    virtual bool initializeAI(bool continueOnErrors);
    virtual bool deinitializeAI(bool continueOnErrors);
    /// Check if Scene::load() was called
    bool checkLoadFlag() const { return _loadComplete; }
    /// Unload scenegraph
    void clearObjects();
    /**End loading and unloading logic*/
    /// returns true if the camera was moved/rotated/etc
    bool updateCameraControls();
    /// Draw debug entities
    virtual void debugDraw(const Camera& activeCamera, RenderStage stage, RenderSubPassCmds& subPassesInOut);

    /// simple function to load the scene elements.
    inline bool SCENE_LOAD(const stringImpl& name,
                           const bool contOnErrorRes,
                           const bool contOnErrorTasks) {
        if (!Scene::load(name)) {
            Console::errorfn(Locale::get(_ID("ERROR_SCENE_LOAD")), "scene load function");
            return false;
        }
        if (!loadResources(contOnErrorRes)) {
            Console::errorfn(Locale::get(_ID("ERROR_SCENE_LOAD")), "scene load resources");
            if (!contOnErrorRes) return false;
        }
        if (!loadTasks(contOnErrorTasks)) {
            Console::errorfn(Locale::get(_ID("ERROR_SCENE_LOAD")), "scene load tasks");
            if (!contOnErrorTasks) return false;
        }
        if (!loadPhysics(contOnErrorTasks)) {
            Console::errorfn(Locale::get(_ID("ERROR_SCENE_LOAD")), "scene load physics");
            if (!contOnErrorTasks) return false;
        }

        registerInputActions();
        loadKeyBindings();

        return true;
    }

   protected:
       /// Global info
       GFXDevice& _GFX;
       ParamHandler&  _paramHandler;
       SceneGraph*    _sceneGraph;
       AI::AIManager* _aiManager;
       SceneGUIElements* _GUI;

       U64 _sceneTimer;
       vectorImpl<D64> _taskTimers;
       vectorImpl<D64> _guiTimers;
       /// Datablocks for models,vegetation,terrains,tasks etc
       FileDataStack _modelDataArray;
       vectorImpl<FileData> _vegetationDataArray;

       vectorImpl<stringImpl> _terrainList;
       vectorImpl<TerrainDescriptor*> _terrainInfoArray;
       F32 _LRSpeedFactor;
       /// Current selection
       SceneGraphNode_wptr _currentSelection;
       SceneGraphNode_wptr _currentHoverTarget;
       SceneGraphNode_wptr _currentSky;
       SceneGraphNode_wptr _flashLight;

       /// Scene::load must be called by every scene. Add a load flag to make sure!
       bool _loadComplete;
       /// Schedule a scene graph parse with the physics engine to recreate/recheck
       /// the collision meshes used by each node
       bool _cookCollisionMeshesScheduled;
       ///_aiTask is the thread handling the AIManager. It is started before each scene's "initializeAI" is called
       /// It is destroyed after each scene's "deinitializeAI" is called
       std::thread _aiTask;

   private:
       vectorImpl<TaskHandle> _tasks;
       /// Contains all game related info for the scene (wind speed, visibility ranges, etc)
       SceneState* _sceneState;
       vectorImpl<DELEGATE_CBK<> > _selectionChangeCallbacks;
       vectorImpl<SceneGraphNode_cwptr> _sceneSelectionCandidates;

   protected:
       LightPool* _lightPool;
       SceneInput* _input;
       PhysicsSceneInterface* _pxScene;
       SceneEnvironmentProbePool* _envProbePool;

       IMPrimitive* _linesPrimitive;
       vectorImpl<IMPrimitive*> _octreePrimitives;
       vectorImpl<BoundingBox> _octreeBoundingBoxes;
};

namespace Attorney {
class SceneManager {
   private:
    static LightPool* lightPool(Scene& scene) {
        return scene._lightPool;
    }

    static bool updateCameraControls(Scene& scene) {
        return scene.updateCameraControls();
    }

    static bool checkLoadFlag(const Scene& scene) {
        return scene.checkLoadFlag();
    }

    static bool deinitializeAI(Scene& scene) {
        return scene.deinitializeAI(true);
    }

    /// Draw debug entities
    static void debugDraw(Scene& scene, const Camera& activeCamera, RenderStage stage, RenderSubPassCmds& subPassesInOut) {
        scene.debugDraw(activeCamera, stage, subPassesInOut);
    }

    static bool frameStarted(Scene& scene) { return scene.frameStarted(); }
    static bool frameEnded(Scene& scene) { return scene.frameEnded(); }
    static bool load(Scene& scene, const stringImpl& name) {
        return scene.load(name);
    }
    static bool unload(Scene& scene) { 
        return scene.unload();
    }

    static void postLoad(Scene& scene) {
        scene.postLoad();
    }

    static void postLoadMainThread(Scene& scene) {
        scene.postLoadMainThread();
    }

    static void onSetActive(Scene& scene) {
        scene.onSetActive();
    }

    static void onRemoveActive(Scene& scene) {
        scene.onRemoveActive();
    }

    static bool onStartup() {
        return Scene::onStartup();
    }

    static bool onShutdown() {
        return Scene::onShutdown();
    }

    static SceneGUIElements* gui(Scene& scene) {
        return scene._GUI;
    }

    friend class Divide::SceneManager;
};

class SceneRenderPass {
 private:
    static SceneEnvironmentProbePool* getEnvProbes(Scene& scene) {
        return scene._envProbePool;
    }

    friend class Divide::RenderPass;
};

class SceneLoadSave {
 private:
    static bool save(const Scene& scene, ByteBuffer& outputBuffer) {
        return scene.save(outputBuffer);
    }

    static bool load(Scene& scene, ByteBuffer& inputBuffer) {
        return scene.load(inputBuffer);
    }

    friend class Divide::LoadSave;
};

class SceneGraph {
private:
    static void onNodeDestroy(Scene& scene, SceneGraphNode& node) {
        if (!scene.getCurrentSelection().expired() &&
            scene.getCurrentSelection().lock()->getGUID() == node.getGUID())
        {
            scene.resetSelection();
        }
    }
    friend class Divide::SceneGraph;
};

class SceneGUI {
private:
    static SceneGUIElements* guiElements(Scene& scene) {
        return scene._GUI;
    }

    friend class Divide::GUI;
};

};  // namespace Attorney
};  // namespace Divide

#endif
