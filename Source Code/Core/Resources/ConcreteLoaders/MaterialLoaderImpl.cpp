#include "Core/Resources/Headers/ResourceLoader.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Geometry/Material/Headers/Material.h"
#include "Geometry/Shapes/Headers/Object3D.h"

namespace Divide {

Material* ImplResourceLoader<Material>::operator()() {
    Material* ptr = MemoryManager_NEW Material();
    assert(ptr != nullptr);

    if (!load(ptr, _descriptor.getName())) {
        MemoryManager::DELETE(ptr);
    } else {
        if (_descriptor.getFlag()) {
            ptr->setShaderProgram("", true);
        }
        if (_descriptor.getEnumValue() ==
            to_uint(Object3D::ObjectFlag::OBJECT_FLAG_SKINNED)) {
            ptr->setHardwareSkinning(true);
        }
    }

    return ptr;
}

DEFAULT_LOADER_IMPL(Material)
};