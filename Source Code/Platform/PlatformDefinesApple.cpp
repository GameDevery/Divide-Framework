#include "stdafx.h"

#if defined(__APPLE_CC__)

#include "Headers/PlatformDefinesApple.h"

#include <SDL_syswm.h>
#include <signal.h

void* malloc_aligned(const size_t size, size_t alignment) {
	return malloc(size);
}

void  malloc_free(void*& ptr) {
	free(ptr);
}

namespace Divide {

    void DebugBreak() {
#if defined(SIGTRAP)
        raise(SIGTRAP)
#else
        raise(SIGABRT)
#endif
    }

    ErrorCode PlatformInitImpl(int argc, char** argv) noexcept {
        return ErrorCode::NO_ERR;
    }

    bool PlatformCloseImpl() noexcept {
        return true;
    }

    bool GetAvailableMemory(SysInfo& info) {
        I32 mib[2] = { CTL_HW, HW_MEMSIZE };
        U32 namelen = sizeof(mib) / sizeof(mib[0]);
        U64 size;
        size_t len = sizeof(size);
        if (sysctl(mib, namelen, &size, &len, NULL, 0) < 0) {
            perror("sysctl");
        } else {
            info._availableRam = to_size(size);
        }

        return true;
    }

    void getWindowHandle(void* window, WindowHandle& handleOut) noexcept {
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(static_cast<SDL_Window*>(window), &wmInfo);

        handleOut._handle = wmInfo.info.cocoa.window;
    }

    void setThreadName(std::thread* thread, const char* threadName) noexcept {
        auto handle = thread->native_handle();
        pthread_setname_np(handle, threadName);
    }

    bool createDirectory(const char* path) {
        int ret = mkdir(path, 0777);
        if (ret != 0) {
            return errno == EEXIST;
        }

        return true;
    }

    #include <sys/prctl.h>
    void setThreadName(const char* threadName) noexcept {
        prctl(PR_SET_NAME, threadName, 0, 0, 0);
    }

    FileWithPath getExecutableLocation(I32 argc, char** argv) {
        ACKNOWLEDGE_UNUSED(argc);
        char buf[1024] = { 0 };
        uint32_t size = sizeof(buf);
        int ret = _NSGetExecutablePath(buf, &size);
        if (0 != ret)
        {
            return getExecutableLocation(argv0);
        }

        return splitPathToNameAndLocation(buf);
    }

}; //namespace Divide

#endif //defined(__APPLE_CC__)
