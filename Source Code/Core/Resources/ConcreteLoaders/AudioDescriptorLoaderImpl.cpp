#include "Core/Resources/Headers/ResourceLoader.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Platform/Audio/Headers/AudioDescriptor.h"

namespace Divide {

template <>
Resource_ptr ImplResourceLoader<AudioDescriptor>::operator()() {
    AudioDescriptor_ptr ptr(MemoryManager_NEW AudioDescriptor(_descriptor.getName(),
                                                              _descriptor.getResourceName(),
                                                              _descriptor.getResourceLocation()),
                            DeleteResource(_cache));
    if (!load(ptr)) {
        ptr.reset();
    } else {
        ptr->isLooping() = _descriptor.getFlag();
    }

    return ptr;
}

};
