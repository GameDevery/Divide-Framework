#include "stdafx.h"

#include "Headers/ResourceCache.h"

#include "Core/Headers/PlatformContext.h"

namespace Divide {

namespace {
    SharedMutex g_hashLock;
    std::set<size_t> g_loadingHashes;
};

ResourceLoadLock::ResourceLoadLock(const size_t hash, PlatformContext& context)
    : _loadingHash(hash),
      _threaded(!Runtime::isMainThread())
{
    while (!SetLoading(_loadingHash)) {
        if (_threaded) {
            notifyTaskPool(context);
        }
    };
}

ResourceLoadLock::~ResourceLoadLock()
{
    const bool ret = SetLoadingFinished(_loadingHash);
    DIVIDE_ASSERT(ret, "ResourceLoadLock failed to remove a resource lock!");
}

bool ResourceLoadLock::IsLoading(const size_t hash) {
    SharedLock<SharedMutex> r_lock(g_hashLock);
    return g_loadingHashes.find(hash) != std::cend(g_loadingHashes);
}

bool ResourceLoadLock::SetLoading(const size_t hash) {
    if (!IsLoading(hash)) {
        UniqueLock<SharedMutex> w_lock(g_hashLock);
        //Check again
        if (g_loadingHashes.find(hash) == std::cend(g_loadingHashes)) {
            g_loadingHashes.insert(hash);
            return true;
        }
    }
    return false;
}

bool ResourceLoadLock::SetLoadingFinished(const size_t hash) {
    UniqueLock<SharedMutex> w_lock(g_hashLock);
    const size_t prevSize = g_loadingHashes.size();
    g_loadingHashes.erase(hash);
    return prevSize > g_loadingHashes.size();
}

void ResourceLoadLock::notifyTaskPool(PlatformContext& context) {
    context.taskPool(TaskPoolType::HIGH_PRIORITY).threadWaiting();
}

void DeleteResource::operator()(CachedResource* res) const
{
    WAIT_FOR_CONDITION(res->getState() == ResourceState::RES_LOADED, false);

    _context->remove(res);

    if (res) {
        if (res->resourceType() != ResourceType::GPU_OBJECT) {
            MemoryManager::DELETE(res);
        } else {
            //?
        }
    }
}

ResourceCache::ResourceCache(PlatformContext& context)
    : PlatformContextComponent(context)
{
}

ResourceCache::~ResourceCache()
{
    Console::printfn(Locale::get(_ID("RESOURCE_CACHE_DELETE")));
    clear();
}

void ResourceCache::printContents() const {
    SharedLock<SharedMutex> r_lock(_creationMutex);
    for (ResourceMap::const_iterator it = std::cbegin(_resDB); it != std::cend(_resDB); ++it) {
        assert(!it->second.expired());

        const CachedResource_ptr res = it->second.lock();
        Console::printfn(Locale::get(_ID("RESOURCE_INFO")), res->resourceName().c_str(), res->getGUID());

    }
}

void ResourceCache::clear() {
    Console::printfn(Locale::get(_ID("STOP_RESOURCE_CACHE")));

    UniqueLock<SharedMutex> w_lock(_creationMutex);
    for (ResourceMap::iterator it = std::begin(_resDB); it != std::end(_resDB); ++it) {
        assert(!it->second.expired());

        const CachedResource_ptr res = it->second.lock();
        if (res->resourceType() != ResourceType::GPU_OBJECT) {
            Console::warnfn(Locale::get(_ID("WARN_RESOURCE_LEAKED")), res->resourceName().c_str(), res->getGUID());
        }
    }

    _resDB.clear();
}

void ResourceCache::add(CachedResource_wptr resource, bool overwriteEntry) {
    DIVIDE_ASSERT(!resource.expired(), Locale::get(_ID("ERROR_RESOURCE_CACHE_LOAD_RES")));

    const CachedResource_ptr res = resource.lock();
    const size_t hash = res->descriptorHash();
    DIVIDE_ASSERT(hash != 0, "ResourceCache add error: Invalid resource hash!");

    Console::printfn(Locale::get(_ID("RESOURCE_CACHE_ADD")), res->resourceName().c_str(), res->getResourceTypeName(), res->getGUID(), hash);

    UniqueLock<SharedMutex> w_lock(_creationMutex);
    const auto ret = _resDB.emplace(hash, res);
    if (!ret.second && overwriteEntry) {
         _resDB[hash] = res;
    }
    DIVIDE_ASSERT(ret.second || overwriteEntry, Locale::get(_ID("ERROR_RESOURCE_CACHE_LOAD_RES")));
}

CachedResource_ptr ResourceCache::find(const size_t descriptorHash, bool& entryInMap) {
    /// Search in our resource cache
    SharedLock<SharedMutex> r_lock(_creationMutex);
    const ResourceMap::const_iterator it = _resDB.find(descriptorHash);
    if (it != std::end(_resDB)) {
        entryInMap = true;
        return it->second.lock();
    }

    entryInMap = false;
    return nullptr;
}

void ResourceCache::remove(CachedResource* resource) {
    WAIT_FOR_CONDITION(resource->getState() != ResourceState::RES_LOADING);
    resource->setState(ResourceState::RES_UNLOADING);

    const size_t resourceHash = resource->descriptorHash();
    const Str256& name = resource->resourceName();
    const I64 guid = resource->getGUID();

    DIVIDE_ASSERT(resourceHash != 0, Locale::get(_ID("ERROR_RESOURCE_CACHE_INVALID_NAME")));

    bool resDBEmpty;
    {
        SharedLock<SharedMutex> r_lock(_creationMutex);
        resDBEmpty = _resDB.empty();
        const auto& it = _resDB.find(resourceHash);
        const auto& huh= it->second;

        const bool expired = huh.expired();
        DIVIDE_ASSERT(!resDBEmpty &&  it != std::end(_resDB), Locale::get(_ID("ERROR_RESOURCE_CACHE_UNKNOWN_RESOURCE")));
    }


    Console::printfn(Locale::get(_ID("RESOURCE_CACHE_REM_RES")), name.c_str(), resourceHash);
    if (!resource->unload()) {
        Console::errorfn(Locale::get(_ID("ERROR_RESOURCE_REM")), name.c_str(), guid);
    }

    if (resDBEmpty) {
        Console::errorfn(Locale::get(_ID("RESOURCE_CACHE_REMOVE_NO_DB")), name.c_str());
    } else {
        UniqueLock<SharedMutex> w_lock(_creationMutex);
        _resDB.erase(_resDB.find(resourceHash));
    }
    resource->setState(ResourceState::RES_UNKNOWN);
}

};
