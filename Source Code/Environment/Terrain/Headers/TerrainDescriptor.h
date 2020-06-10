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
#ifndef _TERRAIN_DESCRIPTOR_H_
#define _TERRAIN_DESCRIPTOR_H_

#include "Core/Resources/Headers/ResourceDescriptor.h"
#include "Utility/Headers/XMLParser.h"

namespace Divide {

class TerrainDescriptor final : public PropertyDescriptor {
   public:
        enum class WireframeMode : U8 {
            NONE = 0,
            EDGES,
            NORMALS,
            COUNT
        };

        enum class ParallaxMode : U8 {
            NONE = 0,
            NORMAL,
            OCCLUSION
        };
   public:
    explicit TerrainDescriptor(const stringImpl& name) noexcept;
    virtual ~TerrainDescriptor();

    bool loadFromXML(const boost::property_tree::ptree& pt, const stringImpl& name);

    void addVariable(const stringImpl& name, const stringImpl& value);
    void addVariable(const stringImpl& name, F32 value);
    stringImpl getVariable(const stringImpl& name) const {
        const hashMap<U64, stringImpl>::const_iterator it = _variables.find(_ID(name.c_str()));
        if (it != std::end(_variables)) {
            return it->second;
        }
        return "";
    }

    F32 getVariablef(const stringImpl& name) const {
        const hashMap<U64, F32>::const_iterator it = _variablesf.find(_ID(name.c_str()));
        if (it != std::end(_variablesf)) {
            return it->second;
        }
        return 0.0f;
    }

    inline U32 maxNodesPerStage() const noexcept {
        // Quadtree, so assume worst case scenario
        return dimensions().maxComponent() / 4;
    }

    size_t getHash() const noexcept final {
        _hash = PropertyDescriptor::getHash();
        for (hashMap<U64, stringImpl>::value_type it : _variables) {
            Util::Hash_combine(_hash, it.first);
            Util::Hash_combine(_hash, it.second);
        }
        for (hashMap<U64, F32>::value_type it : _variablesf) {
            Util::Hash_combine(_hash, it.first);
            Util::Hash_combine(_hash, it.second);
        }
        Util::Hash_combine(_hash, _active);
        Util::Hash_combine(_hash, _textureLayers);
        Util::Hash_combine(_hash, _altitudeRange.x);
        Util::Hash_combine(_hash, _altitudeRange.y);
        Util::Hash_combine(_hash, _chunkSize);
        Util::Hash_combine(_hash, _dimensions.x);
        Util::Hash_combine(_hash, _dimensions.y);
        Util::Hash_combine(_hash, to_base(_wireframeDebug));
        Util::Hash_combine(_hash, to_base(_parallaxMode));

        return _hash;
    }

private:
    hashMap<U64, stringImpl> _variables;
    hashMap<U64, F32> _variablesf;

protected:
    friend class Terrain;

    //x - chunk size, y - patch size in meters
    PROPERTY_RW(vec2<F32>, altitudeRange);
    PROPERTY_RW(vec2<U16>, dimensions, { 1 });
    PROPERTY_RW(U16, chunkSize);
    PROPERTY_RW(WireframeMode, wireframeDebug, WireframeMode::NONE);
    PROPERTY_RW(ParallaxMode, parallaxMode, ParallaxMode::NONE);
    PROPERTY_RW(U8, textureLayers, 1u);
    PROPERTY_RW(bool, active, false);

};

};  // namespace Divide

#endif