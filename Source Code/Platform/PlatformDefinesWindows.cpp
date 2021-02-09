#include "stdafx.h"

#if defined(_WIN32)

#if defined(_DEBUG)
//#include <float.h>
//unsigned int fp_control_state = _controlfp(_EM_INEXACT, _MCW_EM);
#endif

#include "Headers/PlatformDefinesWindows.h"
#include "Core/Headers/StringHelper.h"

#include "Platform/File/Headers/FileManagement.h"

#include <direct.h>

#pragma comment(lib, "OpenAL32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

#pragma comment(lib, "PhysX_64.lib")
#pragma comment(lib, "PhysXCooking_64.lib")
#pragma comment(lib, "PhysXFoundation_64.lib")
#pragma comment(lib, "PhysXPvdSDK_static_64.lib")
#pragma comment(lib, "PhysXExtensions_static_64.lib")

#ifdef _DEBUG
#pragma comment(lib, "DbgHelp.lib")
#pragma comment(lib, "glbindingd.lib")
#pragma comment(lib, "glbinding-auxd.lib")
#pragma comment(lib, "assimp-vc142-mtd.lib")
#pragma comment(lib, "IL_d.lib")
#pragma comment(lib, "ILU_d.lib")
#pragma comment(lib, "libpng_d.lib")
#pragma comment(lib, "jpeg_d.lib")
#pragma comment(lib, "libmng_d.lib")
#pragma comment(lib, "zlib_d.lib")
#pragma comment(lib, "freetype_d.lib")
#pragma comment(lib, "FreeImage_d.lib")
#pragma comment(lib, "SDL2d.lib")
#pragma comment(lib, "SDL2_mixer_d.lib")
#pragma comment(lib, "CEGUIBase-0_d.lib")
#pragma comment(lib, "CEGUICommonDialogs-0_d.lib")
#pragma comment(lib, "CEGUICoreWindowRendererSet_d.lib")
#pragma comment(lib, "CEGUILuaScriptModule-0_d.lib")
#pragma comment(lib, "CEGUISTBImageCodec_d.lib")
#pragma comment(lib, "CEGUITinyXMLParser_d.lib")
#else  //_DEBUG
#pragma comment(lib, "glbinding.lib")
#pragma comment(lib, "glbinding-aux.lib")
#pragma comment(lib, "assimp-vc142-mt.lib")
#pragma comment(lib, "IL.lib")
#pragma comment(lib, "ILU.lib")
#pragma comment(lib, "libpng.lib")
#pragma comment(lib, "jpeg.lib")
#pragma comment(lib, "libmng.lib")
#pragma comment(lib, "zlib.lib")
#pragma comment(lib, "freetype.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "SDL2.lib")
#pragma comment(lib, "SDL2_mixer.lib")
#pragma comment(lib, "CEGUIBase-0.lib")
#pragma comment(lib, "CEGUICommonDialogs-0.lib")
#pragma comment(lib, "CEGUICoreWindowRendererSet.lib")
#pragma comment(lib, "CEGUILuaScriptModule-0.lib")
#pragma comment(lib, "CEGUISTBImageCodec.lib")
#pragma comment(lib, "CEGUITinyXMLParser.lib")
#endif  //_DEBUG

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
// SDL redefines WIN32_LEAN_AND_MEAN
#include <SDL_syswm.h>
#endif

extern "C" {
    _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    _declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
}

void* malloc_aligned(const size_t size, const size_t alignment) {
    return _aligned_malloc(size, alignment);
}

void  malloc_free(void*& ptr) {
    _aligned_free(ptr);
}

LRESULT DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) noexcept {
    ACKNOWLEDGE_UNUSED(hWnd);
    ACKNOWLEDGE_UNUSED(uMsg);
    ACKNOWLEDGE_UNUSED(wParam);
    ACKNOWLEDGE_UNUSED(lParam);
    return FALSE;
}

namespace Divide {
    //https://msdn.microsoft.com/en-us/library/windows/desktop/ms679360%28v=vs.85%29.aspx
    static CHAR * GetLastErrorText(CHAR *pBuf, const LONG bufSize) noexcept {
        LPTSTR pTemp = nullptr;

        if (bufSize < 16) {
            if (bufSize > 0) {
                pBuf[0] = '\0';
            }
            return(pBuf);
        }

        const DWORD retSize = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                            FORMAT_MESSAGE_FROM_SYSTEM |
                                            FORMAT_MESSAGE_ARGUMENT_ARRAY,
                                            nullptr,
                                            GetLastError(),
                                            LANG_NEUTRAL,
                                            reinterpret_cast<LPTSTR>(&pTemp),
                                            0,
                                            nullptr);

        if (!retSize || pTemp == nullptr) {
            pBuf[0] = '\0';
        } else {
            pTemp[strlen(pTemp) - 2] = '\0'; //remove cr and newline character
            sprintf(pBuf, "%0.*s (0x%x)", bufSize - 16, pTemp, GetLastError());
            LocalFree(static_cast<HLOCAL>(pTemp));
        }
        return(pBuf);
    }

    void GetWindowHandle(void* window, WindowHandle& handleOut) noexcept {
        SDL_SysWMinfo wmInfo = {};
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(static_cast<SDL_Window*>(window), &wmInfo);
        handleOut._handle = wmInfo.info.win.window;
    }

    void DebugBreak(const bool condition) noexcept {
        if (condition && IsDebuggerPresent()) {
            __debugbreak();
        }
    }

    ErrorCode PlatformInitImpl(int argc, char** argv) noexcept {
        return ErrorCode::NO_ERR;
    }

    bool PlatformCloseImpl() noexcept {
        return true;
    }

    bool GetAvailableMemory(SysInfo& info) {
        MEMORYSTATUSEX status; 
        status.dwLength = sizeof(status);
        const BOOL infoStatus = GlobalMemoryStatusEx(&status);
        if (infoStatus != FALSE) {
            info._availableRam = status.ullAvailPhys;
            return true;
        } else {
            CHAR msgText[256];
            GetLastErrorText(msgText,sizeof(msgText));
            std::cerr << msgText << std::endl;
        }
        return false;
    }


    constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
    typedef struct tagTHREADNAME_INFO
    {
        DWORD dwType; // Must be 0x1000.
        LPCSTR szName; // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags; // Reserved for future use, must be zero.
    } THREADNAME_INFO;
#pragma pack(pop)

    void SetThreadName(const U32 threadID, const char* threadName) noexcept {
        // DWORD dwThreadID = ::GetThreadId( static_cast<HANDLE>( t.native_handle() ) );

        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = threadName;
        info.dwThreadID = threadID;
        info.dwFlags = 0;

        __try
        {
            RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    void SetThreadName(const char* threadName) noexcept {
        SetThreadName(GetCurrentThreadId(), threadName);
    }

    void SetThreadName(std::thread* thread, const char* threadName) noexcept {
        const DWORD threadId = GetThreadId(static_cast<HANDLE>(thread->native_handle()));
        SetThreadName(threadId, threadName);
    }

    bool CallSystemCmd(const char* cmd, const char* args) {
        STARTUPINFO si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        const stringImpl commandLine = Util::StringFormat("\"%s\" %s", cmd, args);
        char* lpCommandLine = const_cast<char*>(commandLine.c_str());

        const BOOL ret = CreateProcess(nullptr, lpCommandLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return ret == TRUE;
    }
}; //namespace Divide

#endif //defined(_WIN32)