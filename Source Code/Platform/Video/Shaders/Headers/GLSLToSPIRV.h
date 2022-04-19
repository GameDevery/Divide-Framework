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
#ifndef _GLSL_TO_SPIR_V_H_
#define _GLSL_TO_SPIR_V_H_

#include <glslang/SPIRV/GlslangToSpv.h>
#include <Vulkan/vulkan.hpp>

namespace Divide {
    namespace Reflection {
        struct Data;
    };
};

struct SpirvHelper
{
    static void Init();

    static void Finalize();
    static void InitResources(TBuiltInResource& Resources);
    static EShLanguage FindLanguage(const vk::ShaderStageFlagBits shader_type);

    static bool GLSLtoSPV(const vk::ShaderStageFlagBits shader_type, const char* pshader, std::vector<unsigned int>& spirv, const bool targetVulkan, Divide::Reflection::Data& reflectionDataInOut);
    static void BuildReflectionData(glslang::TProgram& program, Divide::Reflection::Data& reflectionDataInOut);
};

#endif //_GLSL_TO_SPIR_V_H_
