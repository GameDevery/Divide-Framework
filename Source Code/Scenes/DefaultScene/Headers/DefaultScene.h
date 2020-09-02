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

#pragma once
#ifndef _DEFAULT_SCENE_H_
#define _DEFAULT_SCENE_H_

#include "Scenes/Headers/Scene.h"

namespace Divide {

class DefaultScene : public Scene {
public:
    DefaultScene(PlatformContext& context, ResourceCache* cache, SceneManager& parent, const Str256& name);

    bool load(const Str256& name) override;
    void postLoadMainThread(const Rect<U16>& targetRenderViewport) override;
    void processInput(PlayerIndex idx, U64 deltaTimeUS) override;
    void processTasks(U64 deltaTimeUS) override;
    void processGUI(U64 deltaTimeUS) override;

    void onSetActive() override;
private:
    void loadScene(I64 btnGUID);

private:
    hashMap<U8, Angle::DEGREES<F32>> _camAngle;
    hashMap<I64, Str256> _buttonToSceneMap;
    Str256 _sceneToLoad;
};
};

#endif //_DEFAULT_SCENE_H_
