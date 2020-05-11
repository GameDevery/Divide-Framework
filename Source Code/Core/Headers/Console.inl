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

#ifndef _CORE_CONSOLE_INL_
#define _CORE_CONSOLE_INL_

#if !defined(if_constexpr)
#if !defined(CPP_17_SUPPORT)
#warning "Constexpr if statement in non C++17 code. Consider updating language version for current project"
#define if_constexpr if
#else
#define if_constexpr if constexpr
#endif
#endif

struct sink { template<typename ...Args> sink(Args const& ...) {} };

namespace Divide {
template <typename... Args>
NO_INLINE void Console::d_printfn(const char* format, Args&&... args) {
    if_constexpr(Config::Build::IS_DEBUG_BUILD) {
        printfn(format, std::forward<Args>(args)...);
    } else {
        sink{ format, args ... };
    }
}

template <typename... Args>
NO_INLINE void Console::d_printf(const char* format, Args&&... args) {
    if_constexpr(Config::Build::IS_DEBUG_BUILD) {
        printf(format, std::forward<Args>(args)...);
    } else {
        sink{ format, args ... };
    }
}

template <typename... Args>
NO_INLINE void Console::d_warnfn(const char* format, Args&&... args) {
    if_constexpr(Config::Build::IS_DEBUG_BUILD) {
        warnfn(format, std::forward<Args>(args)...);
    } else {
        sink{ format, args ... };
    }
}

template <typename... Args>
NO_INLINE void Console::d_warnf(const char* format, Args&&... args) {
    if_constexpr(Config::Build::IS_DEBUG_BUILD) {
        warnf(format, std::forward<Args>(args)...);
    } else {
        sink{ format, args ... };
    }
}

template <typename... Args>
NO_INLINE void Console::d_errorfn(const char* format, Args&&... args) {
    if_constexpr (Config::Build::IS_DEBUG_BUILD) {
        errorfn(format, std::forward<Args>(args)...);
    } else {
        sink{ format, args ... };
    }
}

template <typename... Args>
NO_INLINE void Console::d_errorf(const char* format, Args&&... args) {
    if_constexpr (Config::Build::IS_DEBUG_BUILD) {
        errorf(format, std::forward<Args>(args)...);
    } else {
        sink{ format, args ... };
    }
}

template <typename... Args>
NO_INLINE void Console::printfn(const char* format, Args&&... args) {
    output(formatText(format, std::forward<Args>(args)...), true, EntryType::Info);
}

template <typename... Args>
NO_INLINE void Console::printf(const char* format, Args&&... args) {
    output(formatText(format, std::forward<Args>(args)...), false, EntryType::Info);
}

template <typename... Args>
NO_INLINE void Console::warnfn(const char* format, Args&&... args) {
    output(formatText(format, std::forward<Args>(args)...), true, EntryType::Warning);
}

template <typename... Args>
NO_INLINE void Console::warnf(const char* format, Args&&... args) {
    output(formatText(format, std::forward<Args>(args)...), false, EntryType::Warning);
}

template <typename... Args>
NO_INLINE void Console::errorfn(const char* format, Args&&... args) {
    output(formatText(format, std::forward<Args>(args)...), true, EntryType::Error);
}

template <typename... Args>
NO_INLINE void Console::errorf(const char* format, Args&&... args) {
    output(formatText(format, std::forward<Args>(args)...), false, EntryType::Error);
}

template <typename... Args>
NO_INLINE void Console::printfn(std::ofstream& outStream, const char* format, Args&&... args) {
    output(outStream, formatText(format, std::forward<Args>(args)...), true, EntryType::Info);
}

template <typename... Args>
NO_INLINE void Console::printf(std::ofstream& outStream, const char* format, Args&&... args) {
    output(outStream, formatText(format, std::forward<Args>(args)...), false, EntryType::Info);
}

template <typename... Args>
NO_INLINE void Console::warnfn(std::ofstream& outStream, const char* format, Args&&... args) {
    output(outStream, formatText(format, std::forward<Args>(args)...), true, EntryType::Warning);
}

template <typename... Args>
NO_INLINE void Console::warnf(std::ofstream& outStream, const char* format, Args&&... args) {
    output(outStream, formatText(format, std::forward<Args>(args)...), false, EntryType::Warning);
}

template <typename... Args>
NO_INLINE void Console::errorfn(std::ofstream& outStream, const char* format, Args&&... args) {
    output(outStream, formatText(format, std::forward<Args>(args)...), true, EntryType::Error);
}

template <typename... Args>
NO_INLINE void Console::errorf(std::ofstream& outStream, const char* format, Args&&... args) {
    output(outStream, formatText(format, std::forward<Args>(args)...), false, EntryType::Error);
}

template <typename... Args>
NO_INLINE void Console::d_printfn(std::ofstream& outStream, const char* format, Args&&... args) {
    if_constexpr (Config::Build::IS_DEBUG_BUILD) {
        printfn(outStream, format, std::forward<Args>(args)...);
    } else {
        sink{ outStream, format, args ... };
    }
}

template <typename... Args>
NO_INLINE void Console::d_printf(std::ofstream& outStream, const char* format, Args&&... args) {
    if_constexpr (Config::Build::IS_DEBUG_BUILD) {
        printf(outStream, format, std::forward<Args>(args)...);
    } else {
        sink{ outStream, format, args ... };
    }
}

template <typename... Args>
NO_INLINE void Console::d_warnfn(std::ofstream& outStream, const char* format, Args&&... args) {
    if_constexpr (Config::Build::IS_DEBUG_BUILD) {
        warnfn(outStream, format, std::forward<Args>(args)...);
    } else {
        sink{ outStream, format, args ... };
    }
}

template <typename... Args>
NO_INLINE void Console::d_warnf(std::ofstream& outStream, const char* format, Args&&... args) {
    if_constexpr (Config::Build::IS_DEBUG_BUILD) {
        warnf(outStream, format, std::forward<Args>(args)...);
    } else {
        sink{ outStream, format, args ... };
    }
}

template <typename... Args>
NO_INLINE void Console::d_errorfn(std::ofstream& outStream, const char* format, Args&&... args) {
    if_constexpr (Config::Build::IS_DEBUG_BUILD) {
        errorfn(outStream, format, std::forward<Args>(args)...);
    } else {
        sink{ outStream, format, args ... };
    }
}

template <typename... Args>
NO_INLINE void Console::d_errorf(std::ofstream& outStream, const char* format, Args&&... args) {
    if_constexpr (Config::Build::IS_DEBUG_BUILD) {
        errorf(outStream, format, std::forward<Args>(args)...);
    } else {
        sink{ outStream, format, args ... };
    }
}
};

#endif  //_CORE_CONSOLE_INL_