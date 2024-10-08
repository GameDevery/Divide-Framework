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
#ifndef DVD_RENDER_PACKAGE_H_
#define DVD_RENDER_PACKAGE_H_

#include "Platform/Video/Headers/Commands.h"

namespace Divide {

struct RenderPackage
{
    static constexpr U32 INVALID_CMD_OFFSET = U32_MAX;
    static constexpr U8 INVALID_STAGE_INDEX = U8_MAX;

    RenderPackage();
    ~RenderPackage();

    PROPERTY_RW(GFX::BindPipelineCommand, pipelineCmd);
    PROPERTY_RW(GFX::BindShaderResourcesCommand, descriptorSetCmd);
    PROPERTY_RW(GFX::SendPushConstantsCommand, pushConstantsCmd);

    PROPERTY_RW(U32, drawCmdOffset, INVALID_CMD_OFFSET);
    PROPERTY_RW(U8,  stagePassBaseIndex, INVALID_STAGE_INDEX);

private:
    friend Handle<GFX::CommandBuffer> GetCommandBuffer( RenderPackage& pkg );
    friend void Clear( RenderPackage& pkg ) noexcept;

    Handle<GFX::CommandBuffer> _additionalCommands =  INVALID_HANDLE<GFX::CommandBuffer>;

    UniformData _uniforms{};
};

void Clear(RenderPackage& pkg) noexcept;
Handle<GFX::CommandBuffer> GetCommandBuffer(RenderPackage& pkg);
}; // namespace Divide

#endif //DVD_RENDER_PACKAGE_H_
