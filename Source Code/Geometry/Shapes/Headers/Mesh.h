/*
   Copyright (c) 2017 DIVIDE-Studio
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

#ifndef _MESH_H_
#define _MESH_H_

/**
DIVIDE-Engine: 21.10.2010 (Ionut Cava)

Mesh class. This class wraps all of the renderable geometry drawn by the engine.
The only exceptions are: Terrain (including TerrainChunk) and Vegetation.

Meshes are composed of at least 1 submesh that contains vertex data, texture
info and so on.
A mesh has a name, position, rotation, scale and a Boolean value that enables or
disables rendering
across the network and one that disables rendering altogether;

Note: all transformations applied to the mesh affect every submesh that compose
the mesh.
*/

#include "Object3D.h"

struct aiScene;
namespace Divide {

FWD_DECLARE_MANAGED_CLASS(SubMesh);

class Mesh : public Object3D {
   public:
    explicit Mesh(GFXDevice& context,
                  ResourceCache& parentCache,
                  size_t descriptorHash,
                  const stringImpl& name,
                  const stringImpl& resourceName,
                  const stringImpl& resourceLocation,
                  ObjectFlag flag = ObjectFlag::OBJECT_FLAG_NONE);

    virtual ~Mesh();

    void postLoad(SceneGraphNode& sgn);

    void addSubMesh(SubMesh_ptr subMesh);

    void setAnimator(const std::shared_ptr<SceneAnimator>& animator) {
        assert(getObjectFlag(ObjectFlag::OBJECT_FLAG_SKINNED));
        _animator = animator;
    }

    inline std::shared_ptr<SceneAnimator> getAnimator() { 
        assert(getObjectFlag(ObjectFlag::OBJECT_FLAG_SKINNED));
        return _animator; 
    }

   protected:
    /// Called from SceneGraph "sceneUpdate"
    virtual void sceneUpdate(const U64 deltaTime,
                             SceneGraphNode& sgn,
                             SceneState& sceneState);
    void updateBoundsInternal(SceneGraphNode& sgn) override;

   protected:
    bool _visibleToNetwork;
    /// Animation player to animate the mesh if necessary
    std::shared_ptr<SceneAnimator> _animator;
    vectorImpl<SubMesh_ptr> _subMeshList;
};

TYPEDEF_SMART_POINTERS_FOR_CLASS(Mesh);

};  // namespace Divide

#endif