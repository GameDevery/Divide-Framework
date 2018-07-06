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

#ifndef _CAMERA_MANAGER_H
#define _CAMERA_MANAGER_H

#include "Rendering/Camera/Headers/Camera.h"
#include "Managers/Headers/FrameListenerManager.h"
#include <stack>

namespace Divide {

class Kernel;
/// Multiple camera managers can be created if needed in the future
/// No need for singletons here
class CameraManager : private NonCopyable, public FrameListener {
    typedef hashMapImpl<ULL, Camera*> CameraPool;
    typedef hashMapImpl<I64, Camera*> CameraPoolGUID;

   public:
    CameraManager(Kernel* const kernelPtr);
    ~CameraManager();

    // Camera manager handles the deletion of cameras!
    Camera* createCamera(const stringImpl& cameraName,
                         Camera::CameraType type);

    inline Camera& getActiveCamera() {
        assert(_camera);
        return *_camera;
    }

    inline const Camera& getActiveCamera() const {
        assert(_camera);
        return *_camera;
    }

    void update(const U64 deltaTime);

    void addCameraChangeListener(const DELEGATE_CBK_PARAM<Camera&>& f) {
        _changeCameralisteners.push_back(f);
    }

    void addCameraUpdateListener(const DELEGATE_CBK_PARAM<Camera&>& f) {
        _updateCameralisteners.push_back(f);
        _addNewListener = true;
    }

    inline bool mouseMoved(const Input::MouseEvent& arg) {
        return _camera->mouseMoved(arg);
    }

    inline void pushActiveCamera(const stringImpl& name) {
        pushActiveCamera(_ID_RT(name));
    }

    inline void pushActiveCamera(ULL nameHash) {
        _cameraStack.push(findCamera(nameHash));
        setActiveCamera(_cameraStack.top());
    }

    inline void pushActiveCamera(const Camera* camera) {
        _cameraStack.push(findCamera(_ID_RT(camera->getName())));
        setActiveCamera(_cameraStack.top());
    }

    inline void popActiveCamera() {
        _cameraStack.pop();
        setActiveCamera(_cameraStack.top());
    }

   protected:
    /// This is inherited from FrameListener and is used to update the view
    /// matrix every frame
    bool frameStarted(const FrameEvent& evt);
    Camera* findCamera(ULL nameHash);
    void setActiveCamera(Camera* cam);
    void addNewCamera(const stringImpl& cameraName, Camera* const camera);

   private:
    bool _addNewListener;
    Kernel* _kernelPtr;
    Camera* _camera;
    CameraPool _cameraPool;
    CameraPoolGUID _cameraPoolGUID;
    std::stack<Camera*> _cameraStack;
    vectorImpl<DELEGATE_CBK_PARAM<Camera&> > _changeCameralisteners;
    vectorImpl<DELEGATE_CBK_PARAM<Camera&> > _updateCameralisteners;
};

};  // namespace Divide

#endif