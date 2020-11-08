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
#ifndef _DIVIDE_PCH_
#define _DIVIDE_PCH_

#include "Platform/Headers/PlatformDefinesOS.h"

#if !defined(CPP_VERSION)
#   define CPP_VERSION __cplusplus
#endif

#if CPP_VERSION > 1
#   define CPP_98_SUPPORT
#   define CPP_03_SUPPORT
#   if CPP_VERSION >= 201103L
#       define CPP_11_SUPPORT
#           if CPP_VERSION >= 201402L
#               define CPP_14_SUPPORT
#               if CPP_VERSION > 201402L
#                   if HAS_CPP17
#                       define CPP_17_SUPPORT
#                   endif
#               endif
#           endif
#   endif
#endif 

// As of May 2020
#if !defined(CPP_17_SUPPORT)
#error "Divide Framework requires C++17 support at a minimum!."
#else
#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _ENABLE_EXTENDED_ALIGNED_STORAGE
#define _ENFORCE_MATCHING_ALLOCATORS 0
#endif 

#ifndef BOOST_EXCEPTION_DISABLE
#define BOOST_EXCEPTION_DISABLE
#endif

#ifndef BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE
#define BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE
#endif 

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif 

#ifndef CEGUI_BUILD_STATIC_FACTORY_MODULE
#define CEGUI_BUILD_STATIC_FACTORY_MODULE
#endif

#ifndef TINYXML_STATIC
#define TINYXML_STATIC
#endif


#include <climits>
#include <xmmintrin.h>
#include <cstring>
#include <iomanip>
#include <random>
#include <stack>
#include <any>

#include <boost/intrusive/slist.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/regex.hpp>

#include <simplefilewatcher/includes/FileWatcher.h>
#include <BetterEnums/include/enum.h>
#include <Optick/optick.h>

#define HAVE_M_PI
#define SDL_MAIN_HANDLED
#include <sdl/include/SDL_mixer.h>
#include <sdl/include/SDL.h>

#include <EASTL/set.h>
#include <EASTL/list.h>
#include <EASTL/array.h>
#include <EASTL/stack.h>
#include <EASTL/queue.h>
#include <EASTL/deque.h>
#include <EASTL/vector_map.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/weak_ptr.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/fixed_string.h>
#include <EASTL/unordered_set.h>
#include <MemoryPool/StackAlloc.h>
#include <MemoryPool/C-11/MemoryPool.h>

#include <ArenaAllocator/arena_allocator.h>

#include <ChaiScript/include/chaiscript/chaiscript.hpp>
#include <ChaiScript/include/chaiscript/chaiscript_stdlib.hpp>
#include <ChaiScript/include/chaiscript/utility/utility.hpp>

#include <ConcurrentQueue/concurrentqueue.h>
#include <ConcurrentQueue/blockingconcurrentqueue.h>

#include <Libs/fmt/include/fmt/format.h>
#include <Libs/fmt/include/fmt/printf.h>

#pragma warning(push)
#pragma warning(disable: 4310)
#pragma warning(disable: 4458)
#include <skarupke/bytell_hash_map.hpp>
#pragma warning(pop)

#include "Platform/Headers/PlatformDataTypes.h"
#include "Platform/Threading/Headers/SharedMutex.h"
#include "Platform/Headers/ConditionalWait.h"
#include "Core/Headers/NonCopyable.h"
#include "Core/Headers/NonMovable.h"
#include "Core/Headers/GUIDWrapper.h"

#include "Core/TemplateLibraries/Headers/HashMap.h"
#include "Core/TemplateLibraries/Headers/Vector.h"
#include "Core/TemplateLibraries/Headers/String.h"
#include "Platform/File/Headers/ResourcePath.h"
#include "Core/Math/Headers/MathMatrices.h"
#include "Core/Math/Headers/Quaternion.h"
#include "Core/Headers/TaskPool.h"
#include "Core/Headers/Console.h"

#ifndef YES_IMGUIMINIGAMES
#define YES_IMGUIMINIGAMES
#endif
#define NO_IMGUICODEEDITOR 
#ifndef IMGUI_INCLUDE_IMGUI_USER_H
#define IMGUI_INCLUDE_IMGUI_USER_H
#endif
#ifndef IMGUI_INCLUDE_IMGUI_USER_INL
#define IMGUI_INCLUDE_IMGUI_USER_INL
#endif
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui.h>

#include <ECS.h>

#endif //_DIVIDE_PCH_
