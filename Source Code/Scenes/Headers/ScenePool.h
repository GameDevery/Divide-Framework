/*
Copyright (c) 2018 DIVIDE-Studio
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

#ifndef _SCENE_POOL_H_
#define _SCENE_POOL_H_

#include "Platform/Headers/PlatformDefines.h"

namespace Divide {

class Scene;
class SceneManager;
class ResourceCache;
class PlatformContext;
class ScenePool {
protected:
    SET_DELETE_FRIEND
    friend class SceneManager;
    ScenePool(SceneManager& parentMgr);
    ~ScenePool();

    Scene* getOrCreateScene(PlatformContext& context, ResourceCache& cache, SceneManager& parent, const Str128& name, bool& foundInCache);
    bool   deleteScene(Scene*& scene);

    bool   defaultSceneActive() const;

    Scene&       defaultScene();
    const Scene& defaultScene() const;
    Scene&       activeScene();
    const Scene& activeScene() const;
    void         activeScene(Scene& scene);

    void init();

    std::vector<Str128> sceneNameList(bool sorted) const;

private:
    /// Pointer to the currently active scene
    Scene* _activeScene;
    Scene* _loadedScene;
    Scene* _defaultScene;
    std::vector<Scene*> _createdScenes;

    SceneManager& _parentMgr;

    mutable SharedMutex _sceneLock;
};
}; //namespace Divide

#endif //_SCENE_POOL_H_
