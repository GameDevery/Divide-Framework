

#include "Headers/glMemoryManager.h"
#include "Platform/Video/RenderBackend/OpenGL/Headers/GLWrapper.h"

#include "Core/Headers/StringHelper.h"
#include "Platform/Headers/PlatformRuntime.h"

namespace Divide {
namespace GLUtil {

namespace GLMemory {

namespace
{
    namespace detail
    {
        constexpr size_t zeroDataBaseSize = TO_MEGABYTES(64u);
    };

    eastl::vector<Byte> g_zeroData( detail::zeroDataBaseSize, Byte_ZERO );

}

Byte* GetZeroData( const size_t bufferSize )
{
    while ( g_zeroData.size() < bufferSize )
    {
        g_zeroData.resize( g_zeroData.size() + detail::zeroDataBaseSize, Byte_ZERO );
    }

    return g_zeroData.data();
}

void OnFrameEnd(const U64 frameCount )
{
    constexpr U64 memoryCleanInterval = 256u;

    thread_local U64 lastSyncedFrame = 0u;

    if ( frameCount - lastSyncedFrame > memoryCleanInterval )
    {
        // This may be quite large at this point, so clear it to claim back some RAM
        if (g_zeroData.size() >= detail::zeroDataBaseSize * 4 )
        {
            g_zeroData.set_capacity( 0 );
        }

        lastSyncedFrame = frameCount;
    }
}

Chunk::Chunk(const std::string_view chunkName,
             const size_t alignedSize,
             const gl46core::BufferStorageMask storageMask,
             const gl46core::BufferAccessMask accessMask,
             const U32 flags)
    : _storageMask(storageMask),
      _accessMask(accessMask),
      _flags(flags)
{
    Block& block = _blocks.emplace_back();
    block._size = alignedSize;
    block._pooled = true;

    createAndAllocateMappedBuffer(block._bufferHandle, chunkName, storageMask, block._size, { nullptr, 0u }, accessMask, _memory);

    block._ptr = _memory;
}

Chunk::~Chunk()
{
    DIVIDE_ASSERT( _blocks[0]._bufferHandle > 0u && _blocks[0]._ptr != nullptr );
    
    const gl46core::GLboolean result = gl46core::glUnmapNamedBuffer(_blocks[0]._bufferHandle);
    DIVIDE_ASSERT(result != gl46core::GL_FALSE && "GLUtil::freeBuffer error: buffer unmapping failed");
    gl46core::glDeleteBuffers(1, &_blocks[0]._bufferHandle);
}

void Chunk::deallocate(const Block &block)
{
    PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

    auto it = eastl::find(begin(_blocks), end(_blocks), block);
    DIVIDE_ASSERT(it != cend(_blocks));
    Block* crt = it;

    crt->_free = true;

    ++it;
    while ( it != cend(_blocks) && it->_free)
    {
        crt->_size += it->_size;
        it = _blocks.erase(it);
    }
}

bool Chunk::allocate(const size_t alignedSize, const std::pair<bufferPtr, size_t> initialData, Block &blockOut)
{
    PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

    if (alignedSize > _blocks.back()._size)
    {
        return false;
    }

    const size_t count = _blocks.size();
    for (size_t i = 0u; i < count; ++i)
    {
        Block& block = _blocks[i];
        const size_t remainingSize = block._size;

        if (!block._free || remainingSize < alignedSize)
        {
            continue;
        }

        memcpy(block._ptr, initialData.first, initialData.second);

        block._free = false;
        block._size = alignedSize;
        block._pooled = true;

        blockOut = block;

        if (remainingSize > alignedSize)
        {
            Block nextBlock;
            nextBlock._bufferHandle = block._bufferHandle;

            nextBlock._offset = block._offset + alignedSize;
            nextBlock._size = remainingSize - alignedSize;
            nextBlock._ptr = &_memory[nextBlock._offset];
            nextBlock._pooled = true;

            _blocks.emplace_back(nextBlock);
        }
        return true;
    }

    return false;
}

bool Chunk::containsBlock(const Block &block) const
{
    return eastl::find(begin(_blocks), end(_blocks), block) != cend(_blocks);
}

ChunkAllocator::ChunkAllocator(const size_t size) noexcept
    : _size(size)
{
    assert(size > 0u && isPowerOfTwo(size));
}

std::unique_ptr<Chunk> ChunkAllocator::allocate(const size_t alignedSize,
                                                const gl46core::BufferStorageMask storageMask,
                                                const gl46core::BufferAccessMask accessMask,
                                                const U32 flags) const
{
    PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

    static U32 g_bufferIndex = 0u;

    const size_t overflowSize = to_size(1) << to_size(std::log2(alignedSize) + 1);

    return std::make_unique<Chunk>(Util::StringFormat("DVD_BUFFER_CHUNK_{}", g_bufferIndex++), (alignedSize > _size ? overflowSize : _size), storageMask, accessMask, flags );
}

DeviceAllocator::DeviceAllocator(const GLMemoryType memoryType) noexcept
    : _memoryType(memoryType)
{
}

void DeviceAllocator::init(const size_t size)
{
    deallocate();
    _chunkAllocator = std::make_unique<ChunkAllocator>(size);
}

Block DeviceAllocator::allocate(const bool poolAllocations,
                                const size_t alignedSize,
                                const gl46core::BufferStorageMask storageMask,
                                const gl46core::BufferAccessMask accessMask,
                                const U32 flags,
                                const std::pair<bufferPtr, size_t> initialData)
{
    PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );


    if ( poolAllocations )
    {
        LockGuard<Mutex> w_lock(_chunkAllocatorLock);

        Block block;
        bool failed = true;
        for (auto& chunk : _chunks)
        {
            if (chunk->storageMask() == storageMask && 
                chunk->accessMask() == accessMask &&
                chunk->flags() == flags)
            {
                if (chunk->allocate(alignedSize, initialData, block))
                {
                    failed = false;
                }
            }
        }

        if ( failed )
        {
            auto& newChunk = _chunks.emplace_back(_chunkAllocator->allocate(alignedSize, storageMask, accessMask, flags));
            if(!newChunk->allocate(alignedSize, initialData, block))
            {
                DIVIDE_UNEXPECTED_CALL();
            }
            failed = false;
        }

        return block;
    }

    static U32 g_bufferIndex = 0u;

    LockGuard<Mutex> w_lock(_blockAllocatorLock);
    Block& block = _blocks.emplace_back();
    block._free = false;
    block._pooled = false;
    block._size = alignedSize;
    createAndAllocateMappedBuffer( block._bufferHandle,
                                    Util::StringFormat("DVD_BUFFER_STATIC_{}", g_bufferIndex++),
                                    storageMask,
                                    block._size,
                                    initialData,
                                    accessMask,
                                    block._ptr );
    return block;
}

void DeviceAllocator::deallocate(Block &block)
{
    PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

    if ( block._pooled )
    {
        LockGuard<Mutex> w_lock(_chunkAllocatorLock);

        bool found = false;
        for (auto& chunk : _chunks)
        {
            if (chunk->containsBlock(block))
            {
                chunk->deallocate(block);
                found = true;
                break;
            }
        }
        DIVIDE_ASSERT(found, "DeviceAllocator::deallocate error: unable to deallocate the block");
    }
    else
    {
        {
            LockGuard<Mutex> w_lock(_blockAllocatorLock);
            auto it = eastl::find(begin(_blocks), end(_blocks), block);
            DIVIDE_ASSERT ( it != cend(_blocks) );
            _blocks.erase(it);
        }

        if ( block._ptr != nullptr)
        {
            const gl46core::GLboolean result = gl46core::glUnmapNamedBuffer(block._bufferHandle);
            DIVIDE_ASSERT(result != gl46core::GL_FALSE && "GLUtil::freeBuffer error: buffer unmapping failed");
        }

        GLUtil::freeBuffer(block._bufferHandle);
    }

    block = {};
}

void DeviceAllocator::deallocate()
{
    LockGuard<Mutex> w_lock(_chunkAllocatorLock);
    _chunks.clear();
}

} // namespace GLMemory

void freeBuffer(gl46core::GLuint& bufferId)
{
    PROFILE_SCOPE_AUTO(Profiler::Category::Graphics);

    if (bufferId != GL_NULL_HANDLE && bufferId != 0u)
    {
        if (!GL_API::DeleteBuffers(1, &bufferId))
        {
            DIVIDE_UNEXPECTED_CALL();
        }

        bufferId = GL_NULL_HANDLE;
    }
}

void createBuffer( gl46core::GLuint& bufferIdOut,
                   const std::string_view name )
{
    PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

    GLUtil::freeBuffer(bufferIdOut);
    gl46core::glCreateBuffers(1, &bufferIdOut);

    if constexpr (Config::ENABLE_GPU_VALIDATION)
    {
        gl46core::glObjectLabel( gl46core::GL_BUFFER, bufferIdOut, -1, name.empty() ? Util::StringFormat("DVD_GENERIC_BUFFER_{}", bufferIdOut).c_str() : name.data() );
    }
}


void createAndAllocateBuffer( gl46core::GLuint& bufferIdOut,
                              const std::string_view name,
                              const gl46core::BufferStorageMask storageMask,
                              const size_t alignedSize,
                              const std::pair<bufferPtr, size_t> initialData )
{
    PROFILE_SCOPE_AUTO(Profiler::Category::Graphics);

    createBuffer(bufferIdOut, name);

    DIVIDE_ASSERT(bufferIdOut != 0 && "GLUtil::allocPersistentBuffer error: buffer creation failed");
    const bool hasAllSourceData = initialData.second == alignedSize && initialData.first != nullptr;
    gl46core::glNamedBufferStorage(bufferIdOut, alignedSize, hasAllSourceData ? initialData.first : GLMemory::GetZeroData(alignedSize), storageMask);
}

void createAndAllocateMappedBuffer( gl46core::GLuint& bufferIdOut,
                                    const std::string_view name,
                                    const gl46core::BufferStorageMask storageMask,
                                    const size_t alignedSize,
                                    const std::pair<bufferPtr, size_t> initialData,
                                    const gl46core::BufferAccessMask accessMask,
                                    Byte*& ptrOut)
{
    PROFILE_SCOPE_AUTO(Profiler::Category::Graphics);

    createAndAllocateBuffer(bufferIdOut, name, storageMask, alignedSize, initialData);

    ptrOut = (Byte*)gl46core::glMapNamedBufferRange(bufferIdOut, 0, alignedSize, accessMask);
    DIVIDE_ASSERT(ptrOut != nullptr);

    const bool hasAllSourceData = initialData.second == alignedSize && initialData.first != nullptr;
    if (!hasAllSourceData && initialData.second > 0 && initialData.first != nullptr)
    {
        memcpy(ptrOut, initialData.first, initialData.second);
    }
}
}  // namespace GLUtil
}  // namespace Divide
