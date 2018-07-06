/*
   Copyright (c) 2014 DIVIDE-Studio
   Copyright (c) 2009 Ionut Cava

   This file is part of DIVIDE Framework.

   Permission is hereby granted, free of charge, to any person obtaining a copy of this software
   and associated documentation files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#ifndef _SKINNED_MESH_H_
#define _SKINNED_MESH_H_

#include "Mesh.h"

class SkinnedMesh : public Mesh {
public:
    SkinnedMesh();
    ~SkinnedMesh();

    void sceneUpdate(const U64 deltaTime,SceneGraphNode* const sgn, SceneState& sceneState);

    /// Use playAnimations() to toggle animation playback for the current mesh (and all submeshes) on or off
    inline void playAnimation(const bool state)       {_playAnimation = state;}
    inline bool playAnimation()                 const {return _playAnimation;}

    /// Select next available animation
    bool playNextAnimation();
    /// Select an animation by index
    bool playAnimation(I32 index);
    /// Select an animation by name
    bool playAnimation(const std::string& animationName);

protected:
    bool _playAnimation;
};
#endif;