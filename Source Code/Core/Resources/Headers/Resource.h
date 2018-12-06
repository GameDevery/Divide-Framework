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
#ifndef _RESOURCE_H_
#define _RESOURCE_H_

#include "Core/Math/Headers/MathMatrices.h"

namespace Divide {

/// When "CreateResource" is called, the resource is in "RES_UNKNOWN" state.
/// Once it has been instantiated it will move to the "RES_CREATED" state.
/// Calling "load" on a non-created resource will fail.
/// After "load" is called, the resource is move to the "RES_LOADING" state
/// Nothing can be done to the resource when it's in "RES_LOADING" state!
/// Once loading is complete, preferably in another thread,
/// the resource state will become "RES_LOADED" and ready to be used (e.g. added
/// to the SceneGraph)
/// Calling "unload" is only available for "RES_LOADED" state resources.
/// Calling this method will set the state to "RES_LOADING"
/// Once unloading is complete, the resource will become "RES_CREATED".
/// It will still exist, but won't contain any data.
/// RES_UNKNOWN and RES_CREATED are safe to delete

enum class ResourceState : U8 {
    RES_UNKNOWN = 0,  //<The resource exists, but it's state is undefined
    RES_CREATED = 1,  //<The pointer has been created and instantiated, but no
                      //data has been loaded
    RES_LOADING = 2,  //<The resource is loading or unloading, creating or
                      //deleting data, parsing scripts, etc
    RES_LOADED  = 3,  //<The resource is loaded and available
    COUNT
};

enum class ResourceType : U8 {
    DEFAULT = 0,
    GPU_OBJECT = 1, //< Textures, Render targets, shaders, etc
    COUNT
};

FWD_DECLARE_MANAGED_CLASS(Resource);
FWD_DECLARE_MANAGED_CLASS(CachedResource);

class Resource : public GUIDWrapper
{
   public:
    explicit Resource(ResourceType type,
                      const stringImpl& resourceName);
    virtual ~Resource();

    /// Name management
    const stringImpl& resourceName() const;
    ResourceType getType() const;
    ResourceState getState() const;

   protected:
    virtual void setState(ResourceState currentState);

   protected:
    stringImpl   _resourceName;
    ResourceType _resourceType;
    std::atomic<ResourceState> _resourceState;
};

class CachedResource : public Resource,
                       public std::enable_shared_from_this<CachedResource>
{
    friend class ResourceCache;
    friend class ResourceLoader;
    template <typename X>
    friend class ImplResourceLoader;

public:
    explicit CachedResource(ResourceType type,
                            size_t descriptorHash,
                            const stringImpl& resourceName);
    explicit CachedResource(ResourceType type,
                            size_t descriptorHash,
                            const stringImpl& resourceName,
                            const stringImpl& assetName);
    explicit CachedResource(ResourceType type,
                            size_t descriptorHash,
                            const stringImpl& resourceName,
                            const stringImpl& assetName,
                            const stringImpl& assetLocation);

    virtual ~CachedResource();


    /// Loading and unloading interface
    virtual bool load(const DELEGATE_CBK<void, CachedResource_wptr>& onLoadCallback);
    virtual bool unload();
    virtual bool resourceLoadComplete();

    size_t getDescriptorHash() const;
    /// Physical file location
    const stringImpl& assetLocation() const;
    /// Physical file name
    const stringImpl& assetName() const;

    inline stringImpl assetPath() const { return assetLocation() + "/" + assetName(); }

    void setStateCallback(ResourceState targetState, const DELEGATE_CBK<void, Resource_wptr>& cbk);

protected:
    void setState(ResourceState currentState) override;
    void assetName(const stringImpl& name);
    void assetLocation(const stringImpl& location);

protected:
    size_t _descriptorHash;
    stringImpl   _assetLocation;  ///< Physical file location
    stringImpl   _assetName;      ///< Physical file name
    std::array<DELEGATE_CBK<void, Resource_wptr>, to_base(ResourceState::COUNT)> _loadingCallbacks;
    mutable std::mutex _callbackLock;
};

struct TerrainInfo {
    TerrainInfo() noexcept { position.set(0, 0, 0); }
    /// "variables" contains the various strings needed for each terrain such as
    /// texture names,
    /// terrain name etc.
    hashMap<U64, stringImpl> variables;
    F32 grassDensity = 1.0f;
    F32 treeDensity = 1.0f;
    F32 grassScale = 1.0f;
    F32 treeScale = 1.0f;
    vec3<F32> position;
    vec2<F32> scale;
    bool active = false;
};

FWD_DECLARE_MANAGED_CLASS(Resource);

};  // namespace Divide

#endif