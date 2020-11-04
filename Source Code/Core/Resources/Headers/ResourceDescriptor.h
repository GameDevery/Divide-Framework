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
#ifndef RESOURCE_DESCRIPTOR_H_
#define RESOURCE_DESCRIPTOR_H_

#include "Core/Headers/Hashable.h"

namespace Divide {

/// Dummy class so that resource loaders can have fast access to extra
/// information in a general format
class PropertyDescriptor : public Hashable {
   public:
    enum class DescriptorType : U8 {
        DESCRIPTOR_TEXTURE = 0,
        DESCRIPTOR_PARTICLE,
        DESCRIPTOR_TERRAIN_INFO,
        DESCRIPTOR_SHADER,
        DESCRIPTOR_COUNT
    };

    explicit PropertyDescriptor(const DescriptorType& type) noexcept : _type(type)
    {
    }

    virtual size_t getHash() const noexcept override;

protected:
    /// useful for switch statements
    DescriptorType _type;
};

class CachedResource;
FWD_DECLARE_MANAGED_CLASS(CachedResource);
FWD_DECLARE_MANAGED_CLASS(PropertyDescriptor);

class ResourceDescriptor final : public Hashable {
   public:
       using CBK = DELEGATE<void, CachedResource_wptr>;

    ///resourceName is the name of the resource instance, not an actual asset name! Use "assetName" for that
    explicit ResourceDescriptor(const Str256& resourceName);

    ~ResourceDescriptor() = default;

    template <typename T>
    typename std::enable_if<std::is_base_of<PropertyDescriptor, T>::value, const std::shared_ptr<T>>::type
    propertyDescriptor() const { return std::dynamic_pointer_cast<T>(_propertyDescriptor); }

    template <typename T>
    typename std::enable_if<std::is_base_of<PropertyDescriptor, T>::value, void>::type
    propertyDescriptor(const T& descriptor) { _propertyDescriptor.reset(new T(descriptor)); }

    size_t getHash() const noexcept final;

    PROPERTY_RW(ResourcePath, assetLocation); //<Can't be fixed size due to the need to handle array textures, cube maps, etc
    PROPERTY_RW(ResourcePath, assetName); //< Resource instance name (for lookup)
    PROPERTY_RW(Str256, resourceName);
    PROPERTY_RW(bool, flag, false);
    PROPERTY_RW(bool, threaded, true);
    PROPERTY_RW(bool, waitForReady, true);
    PROPERTY_RW(U32, enumValue, 0u);
    PROPERTY_RW(U32, ID, 0u);
    PROPERTY_RW(P32, mask); //< 4 bool values representing  ... anything ...
    PROPERTY_RW(vec3<U16>, data, VECTOR3_ZERO); //< general data

   private:
    /// Use for extra resource properties: textures, samplers, terrain etc.
    PropertyDescriptor_ptr _propertyDescriptor = nullptr;
};

};  // namespace Divide

#endif
