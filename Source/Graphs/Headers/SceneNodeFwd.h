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
#ifndef DVD_SCENE_NODE_FWD_H_
#define DVD_SCENE_NODE_FWD_H_

#include "Core/Math/Headers/Ray.h"

namespace Divide {

    /// Usage context affects lighting, navigation, physics, etc
    enum class NodeUsageContext : U8
    {
        NODE_DYNAMIC = 0,
        NODE_STATIC
    };

    /// ToDo: Move particle emitter to components (it will make them way more dynamic) - Ionut
    enum class SceneNodeType : U16
    {
        TYPE_SPHERE_3D = 0,
        TYPE_BOX_3D,
        TYPE_QUAD_3D,
        TYPE_MESH,
        TYPE_SUBMESH,
        TYPE_TERRAIN,
        TYPE_TRANSFORM,
        TYPE_WATER,
        TYPE_PARTICLE_EMITTER,
        TYPE_SKY,
        TYPE_INFINITEPLANE,
        TYPE_VEGETATION,
        COUNT
    };

    namespace Names
    {
        extern const char* sceneNodeType[to_base(SceneNodeType::COUNT) + 1u];
    };

    FWD_DECLARE_MANAGED_CLASS(SceneNode);

    struct SGNRayResult
    {
        I64 sgnGUID = -1;
        F32 dist = F32_MAX;
        bool inside = false;
        const char* name = nullptr;
    };

    struct SGNIntersectionParams
    {
        Ray _ray;
        float2 _range = { 0.f, 1.f };
        const SceneNodeType* _ignoredTypes = nullptr;
        size_t _ignoredTypesCount = 0;
        bool _includeTransformNodes = true;
    };

    [[nodiscard]] FORCE_INLINE constexpr bool IsPrimitive( const SceneNodeType type ) noexcept
    {
        return type == SceneNodeType::TYPE_BOX_3D ||
               type == SceneNodeType::TYPE_QUAD_3D ||
               type == SceneNodeType::TYPE_SPHERE_3D;
    }

    [[nodiscard]] FORCE_INLINE constexpr bool IsMesh( const SceneNodeType type ) noexcept
    {
        return type == SceneNodeType::TYPE_MESH ||
               type == SceneNodeType::TYPE_SUBMESH;
    }

    [[nodiscard]] FORCE_INLINE constexpr bool Is3DObject( const SceneNodeType type ) noexcept
    {
        return IsPrimitive(type) ||
               IsMesh(type) ||
               type == SceneNodeType::TYPE_TERRAIN;
    }

    [[nodiscard]] FORCE_INLINE constexpr bool IsTransformNode( const SceneNodeType nodeType ) noexcept
    {
        return nodeType == SceneNodeType::TYPE_TRANSFORM || nodeType == SceneNodeType::TYPE_MESH;
    }

    template<typename T>
    constexpr SceneNodeType GetSceneNodeType() { return SceneNodeType::COUNT; }

    #define DEFINE_NODE_TYPE_OVERLOAD(Name, Enum) \
    class Name; \
    template<> \
    constexpr SceneNodeType GetSceneNodeType<Name>() { return Enum; } \

    #define DEFINE_NODE_TYPE_INHERIT(Name, Enum, Base) \

    #define DEFINE_NODE_TYPE(Name, Enum) \
    DEFINE_NODE_TYPE_OVERLOAD(Name, Enum) \
    class Name final : public SceneNode

    #define DEFINE_NODE_BASE_TYPE(Name, Enum) \
    DEFINE_NODE_TYPE_OVERLOAD(Name, Enum) \
    class Name : public SceneNode

    #define DEFINE_3D_OBJECT_TYPE(Name, Enum) \
    DEFINE_NODE_TYPE_OVERLOAD(Name, Enum) \
    class Name final : public Object3D

}; //namespace Divide

#endif //DVD_SCENE_NODE_FWD_H_
