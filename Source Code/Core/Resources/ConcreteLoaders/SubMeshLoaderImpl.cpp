#include "Core/Headers/PlatformContext.h"
#include "Core/Resources/Headers/ResourceLoader.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Geometry/Shapes/Headers/SkinnedSubMesh.h"

namespace Divide {

template<>
CachedResource_ptr ImplResourceLoader<SubMesh>::operator()() {
    SubMesh_ptr ptr;

    if (_descriptor.getEnumValue() ==
        to_const_uint(Object3D::ObjectFlag::OBJECT_FLAG_SKINNED)) {
        ptr.reset(MemoryManager_NEW SkinnedSubMesh(_context.gfx(), _cache, _loadingDescriptorHash, _descriptor.getName()), DeleteResource(_cache));
    } else {
        ptr.reset(MemoryManager_NEW SubMesh(_context.gfx(), _cache, _loadingDescriptorHash, _descriptor.getName()), DeleteResource(_cache));
    }

    if (!load(ptr, _descriptor.onLoadCallback())) {
        ptr.reset();
    } else {
        if (_descriptor.getFlag()) {
            ptr->renderState().useDefaultMaterial(false);
        }
        ptr->setID(_descriptor.getID());
    }

    return ptr;
}

};
