#include "stdafx.h"

#include "Core/Headers/PlatformContext.h"
#include "Core/Resources/Headers/ResourceLoader.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Environment/Sky/Headers/Sky.h"

namespace Divide {

template<>
CachedResource_ptr ImplResourceLoader<Sky>::operator()() {
    eastl::shared_ptr<Sky> ptr(MemoryManager_NEW Sky(_context.gfx(), _cache, _loadingDescriptorHash, _descriptor.resourceName(), _descriptor.ID()),
                               DeleteResource(_cache));

    if (!load(ptr)) {
        ptr.reset();
    }

    return ptr;
}

};
