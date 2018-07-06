/*
   Copyright (c) 2015 DIVIDE-Studio
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

#ifndef _SCENE_MANAGER_H
#define _SCENE_MANAGER_H

#include "Scenes/Headers/Scene.h"
#include <boost/functional/factory.hpp>

namespace Divide {

enum class RenderStage : U32;
class RenderPassCuller;
namespace Attorney {
    class SceneManagerKernel;
};

DEFINE_SINGLETON_EXT2(SceneManager, FrameListener,
                      Input::InputAggregatorInterface)
    friend class Attorney::SceneManagerKernel;
  public:
    /// Lookup the factory methods table and return the pointer to a newly
    /// constructed scene bound to that name
    Scene* createScene(const stringImpl& name);

    inline Scene& getActiveScene() { return *_activeScene; }
    inline void setActiveScene(Scene& scene) {
        _activeScene.reset(&scene);
    }

    bool init(GUI* const gui);

    /*Base Scene Operations*/
    void preRender();
    void render(RenderStage stage, const Kernel& kernel);
    void postRender();
    // renders the visible nodes
    void renderVisibleNodes(bool flushCache);
    // updates and culls the scene graph to generate visible nodes
    void updateVisibleNodes(bool flushCache);
    inline void onLostFocus() { _activeScene->onLostFocus(); }
    inline void idle() { _activeScene->idle(); }
    bool unloadCurrentScene();
    bool load(const stringImpl& name, const vec2<U16>& resolution);
    /// Check if the scene was loaded properly
    inline bool checkLoadFlag() const {
        return Attorney::SceneManager::checkLoadFlag(*_activeScene);
    }
    /// Create AI entities, teams, NPC's etc
    inline bool initializeAI(bool continueOnErrors) {
        return Attorney::SceneManager::initializeAI(*_activeScene,
                                                  continueOnErrors);
    }
    /// Update animations, network data, sounds, triggers etc.
    inline void updateSceneState(const U64 deltaTime) {
        _activeScene->updateSceneState(deltaTime);
    }

    /// Gather input events and process them in the current scene
    inline void processInput(const U64 deltaTime) {
        _activeScene->processInput(deltaTime);
        Attorney::SceneManager::updateCameraControls(*_activeScene);
    }

    inline void processTasks(const U64 deltaTime) {
        _activeScene->processTasks(deltaTime);
    }
    inline void processGUI(const U64 deltaTime) {
        _activeScene->processGUI(deltaTime);
    }
    /// Insert a new scene factory method for the given name
    template <typename DerivedScene>
    inline bool registerScene(const stringImpl& sceneName) {
        _sceneFactory[sceneName] = boost::factory<DerivedScene*>();
        return true;
    }

    const vectorImpl<SceneGraphNode_wptr>& getVisibleShadowCasters() const {
        return _visibleShadowCasters;
    }

  public:  /// Input
    /// Key pressed
    bool onKeyDown(const Input::KeyEvent& key);
    /// Key released
    bool onKeyUp(const Input::KeyEvent& key);
    /// Joystic axis change
    bool joystickAxisMoved(const Input::JoystickEvent& arg, I8 axis);
    /// Joystick direction change
    bool joystickPovMoved(const Input::JoystickEvent& arg, I8 pov);
    /// Joystick button pressed
    bool joystickButtonPressed(const Input::JoystickEvent& arg,
                               Input::JoystickButton button);
    /// Joystick button released
    bool joystickButtonReleased(const Input::JoystickEvent& arg,
                                Input::JoystickButton button);
    bool joystickSliderMoved(const Input::JoystickEvent& arg, I8 index);
    bool joystickVector3DMoved(const Input::JoystickEvent& arg, I8 index);
    /// Mouse moved
    bool mouseMoved(const Input::MouseEvent& arg);
    /// Mouse button pressed
    bool mouseButtonPressed(const Input::MouseEvent& arg,
                            Input::MouseButton button);
    /// Mouse button released
    bool mouseButtonReleased(const Input::MouseEvent& arg,
                             Input::MouseButton button);

  protected:
    void renderScene();
    void initPostLoadState();
    void onCameraUpdate(Camera& camera);
    void sortVisibleNodes(RenderPassCuller::VisibleNodeCache& nodes) const;

  protected:
    bool frameStarted(const FrameEvent& evt);
    bool frameEnded(const FrameEvent& evt);

  private:
    SceneManager();
    ~SceneManager();

  private:
    typedef hashMapImpl<stringImpl, Scene*> SceneMap;
    bool _init;
    bool _processInput;

    /// The bounding sphere of all of the visible shadow casters in the current frustum
    vectorImpl<SceneGraphNode_wptr> _visibleShadowCasters;
    /// Pointer to the currently active scene
    std::unique_ptr<Scene> _activeScene;
    /// Pointer to the GUI interface
    GUI* _GUI;
    /// Pointer to the scene graph culler that's used to determine what nodes are
    /// visible in the current frame
    RenderPassCuller* _renderPassCuller;
    /// Pointer to the render pass manager
    RenderPassManager* _renderPassManager;
    /// Scene pool
    SceneMap _sceneMap;
    /// Scene_Name -Scene_Factory table
    hashMapImpl<stringImpl, std::function<Scene*()> > _sceneFactory;
    Material* _defaultMaterial;

END_SINGLETON

namespace Attorney {
class SceneManagerKernel {
   private:
    static void initPostLoadState() {
        Divide::SceneManager::getInstance().initPostLoadState();
    }
    static void onCameraUpdate(Camera& camera) {
        Divide::SceneManager::getInstance().onCameraUpdate(camera);
    }
    friend class Divide::Kernel;
};
};  // namespace Attorney

/// Return a pointer to the currently active scene
inline Scene& GET_ACTIVE_SCENE() {
    return SceneManager::getInstance().getActiveScene();
}

/// Return a pointer to the currently active scene's scenegraph
inline SceneGraph& GET_ACTIVE_SCENEGRAPH() {
    return GET_ACTIVE_SCENE().getSceneGraph();
}

};  // namespace Divide

#endif
