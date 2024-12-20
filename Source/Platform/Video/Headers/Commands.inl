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
#ifndef DVD_GFX_COMMAND_INL_
#define DVD_GFX_COMMAND_INL_

#include "CommandBuffer.h"
#include "ClipPlanes.h"
#include "DescriptorSetsFwd.h"
#include "GenericDrawCommand.h"
#include "PushConstants.h"
#include "Platform/Video/Textures/Headers/Texture.h"
#include "Platform/Video/Buffers/RenderTarget/Headers/RenderTarget.h"
#include "Platform/Video/Buffers/VertexBuffer/Headers/BufferLocks.h"
#include "Platform/Video/Headers/DescriptorSets.h"
#include "Rendering/Camera/Headers/CameraSnapshot.h"
#include "Utility/Headers/TextLabel.h"

struct ImDrawData;

namespace Divide {
class Pipeline;
class ShaderBuffer;

namespace GFX {

template<CommandType EnumVal>
void Command<EnumVal>::addToBuffer( CommandBuffer* buffer ) const
{
    using CType = MapToDataType<EnumVal>;
    buffer->add( static_cast<const CType&>(*this) );
}

template<CommandType EnumVal>
void Command<EnumVal>::DeleteCmd( CommandBase*& cmd ) const
{
    using CType = MapToDataType<EnumVal>;
    CmdAllocator<CType>::GetPool().deleteElement( cmd->As<CType>() );
    cmd = nullptr;
}

DEFINE_COMMAND_BEGIN(BindPipelineCommand, CommandType::BIND_PIPELINE);
    const Pipeline* _pipeline = nullptr;
DEFINE_COMMAND_END(BindPipelineCommand);

DEFINE_COMMAND_BEGIN(SendPushConstantsCommand, CommandType::SEND_PUSH_CONSTANTS);
    UniformData* _uniformData = nullptr;
    PushConstantsStruct _fastData{};
DEFINE_COMMAND_END(SendPushConstantsCommand);

DEFINE_COMMAND_BEGIN(DrawCommand, CommandType::DRAW_COMMANDS);
    DrawCommand() {}
    DrawCommand(GenericDrawCommand& cmd) { _drawCommands.push_back(cmd); }
    DrawCommand(GenericDrawCommand&& cmd) { _drawCommands.emplace_back(MOV(cmd)); }
    DrawCommand(GenericDrawCommandContainer& cmds) : _drawCommands(cmds) {}
    DrawCommand(GenericDrawCommandContainer&& cmds) : _drawCommands(MOV(cmds)) {}

    GenericDrawCommandContainer _drawCommands;
DEFINE_COMMAND_END(DrawCommand);

DEFINE_COMMAND_BEGIN(SetViewportCommand, CommandType::SET_VIEWPORT);
    Rect<I32> _viewport;
DEFINE_COMMAND_END(SetViewportCommand);

DEFINE_COMMAND_BEGIN(PushViewportCommand, CommandType::PUSH_VIEWPORT);
    Rect<I32> _viewport;
DEFINE_COMMAND_END(PushViewportCommand);

DEFINE_COMMAND(PopViewportCommand, CommandType::POP_VIEWPORT);

DEFINE_COMMAND_BEGIN(BeginRenderPassCommand, CommandType::BEGIN_RENDER_PASS);
    RenderTargetID _target{ INVALID_RENDER_TARGET_ID };
    RTDrawDescriptor _descriptor{};
    RTClearDescriptor _clearDescriptor{};
    Str<64> _name{};
DEFINE_COMMAND_END(BeginRenderPassCommand);

DEFINE_COMMAND_BEGIN(EndRenderPassCommand, CommandType::END_RENDER_PASS);
    RTTransitionMask _transitionMask = {true, true, true, true, true};
DEFINE_COMMAND_END(EndRenderPassCommand);

DEFINE_COMMAND_BEGIN(BeginGPUQueryCommand, CommandType::BEGIN_GPU_QUERY);
    U32 _queryMask{ 0u };
DEFINE_COMMAND_END(BeginGPUQueryCommand);

DEFINE_COMMAND_BEGIN(EndGPUQueryCommand, CommandType::END_GPU_QUERY);
    QueryResults* _resultContainer{nullptr};
    bool _waitForResults{false};
DEFINE_COMMAND_END(EndGPUQueryCommand);

DEFINE_COMMAND_BEGIN(BlitRenderTargetCommand, CommandType::BLIT_RT);
    RenderTargetID _source{ INVALID_RENDER_TARGET_ID };
    RenderTargetID _destination{ INVALID_RENDER_TARGET_ID };
    RTBlitParams _params{};
DEFINE_COMMAND_END(BlitRenderTargetCommand);

DEFINE_COMMAND_BEGIN(CopyTextureCommand, CommandType::COPY_TEXTURE);
    Handle<Texture> _source{ INVALID_HANDLE<Texture> };
    Handle<Texture> _destination{ INVALID_HANDLE<Texture> };
    U8 _sourceMSAASamples{ 0u };
    U8 _destinationMSAASamples{ 0u };
    CopyTexParams _params;
DEFINE_COMMAND_END(CopyTextureCommand);

DEFINE_COMMAND_BEGIN( ReadTextureCommand, CommandType::READ_TEXTURE );
    Handle<Texture> _texture{ INVALID_HANDLE<Texture> };
    PixelAlignment _pixelPackAlignment{};
    U8 _mipLevel{0u};
    DELEGATE_STD<void, const ImageReadbackData&> _callback;
DEFINE_COMMAND_END( ReadTextureCommand );

DEFINE_COMMAND_BEGIN(ClearTextureCommand, CommandType::CLEAR_TEXTURE);
    Handle<Texture> _texture{ INVALID_HANDLE<Texture> };
    /// r = depth, g = stencil if target is a depth(+stencil) attachment
    UColour4 _clearColour;
    SubRange _layerRange{0u, U16_MAX};
    U8 _mipLevel{ 0u };
DEFINE_COMMAND_END(ClearTextureCommand);

DEFINE_COMMAND_BEGIN(ComputeMipMapsCommand, CommandType::COMPUTE_MIPMAPS);
    Handle<Texture> _texture{ INVALID_HANDLE<Texture> };
    SubRange _layerRange{ 0u, 1u };
    SubRange _mipRange{ 0u, U16_MAX };
    ImageUsage _usage{ ImageUsage::COUNT };
DEFINE_COMMAND_END(ComputeMipMapsCommand);

DEFINE_COMMAND_BEGIN(SetScissorCommand, CommandType::SET_SCISSOR);
    Rect<I32> _rect;
DEFINE_COMMAND_END(SetScissorCommand);

DEFINE_COMMAND_BEGIN(SetCameraCommand, CommandType::SET_CAMERA);
    CameraSnapshot _cameraSnapshot;
DEFINE_COMMAND_END(SetCameraCommand);

DEFINE_COMMAND_BEGIN(PushCameraCommand, CommandType::PUSH_CAMERA);
    CameraSnapshot _cameraSnapshot;
DEFINE_COMMAND_END(PushCameraCommand);

DEFINE_COMMAND(PopCameraCommand, CommandType::POP_CAMERA);

DEFINE_COMMAND_BEGIN(SetClipPlanesCommand, CommandType::SET_CLIP_PLANES);
    FrustumClipPlanes _clippingPlanes;
DEFINE_COMMAND_END(SetClipPlanesCommand);

DEFINE_COMMAND_BEGIN(BindShaderResourcesCommand, CommandType::BIND_SHADER_RESOURCES);
    DescriptorSet _set;
    DescriptorSetUsage _usage{ DescriptorSetUsage::COUNT };
DEFINE_COMMAND_END(BindShaderResourcesCommand);

DEFINE_COMMAND_BEGIN(BeginDebugScopeCommand, CommandType::BEGIN_DEBUG_SCOPE);
    Str<64> _scopeName;
    U32 _scopeId{ U32_MAX };
DEFINE_COMMAND_END(BeginDebugScopeCommand);

DEFINE_COMMAND(EndDebugScopeCommand, CommandType::END_DEBUG_SCOPE);

DEFINE_COMMAND_BEGIN(AddDebugMessageCommand, CommandType::ADD_DEBUG_MESSAGE);
    Str<64> _msg;
    U32 _msgId{ U32_MAX };
DEFINE_COMMAND_END(AddDebugMessageCommand);

DEFINE_COMMAND_BEGIN(DispatchShaderTaskCommand, CommandType::DISPATCH_SHADER_TASK);
    uint3 _workGroupSize;
DEFINE_COMMAND_END(DispatchShaderTaskCommand);

DEFINE_COMMAND_BEGIN(MemoryBarrierCommand, CommandType::MEMORY_BARRIER);
    BufferLocks _bufferLocks;
    TextureLayoutChanges _textureLayoutChanges;
DEFINE_COMMAND_END(MemoryBarrierCommand);

DEFINE_COMMAND_BEGIN(ReadBufferDataCommand, CommandType::READ_BUFFER_DATA);
    ShaderBuffer* _buffer{ nullptr };
    std::pair<bufferPtr, size_t> _target { nullptr, 0u };
    U32           _offsetElementCount{ 0 };
    U32           _elementCount{ 0 };
DEFINE_COMMAND_END(ReadBufferDataCommand);

DEFINE_COMMAND_BEGIN(ClearBufferDataCommand, CommandType::CLEAR_BUFFER_DATA);
    ShaderBuffer* _buffer{ nullptr };
    U32           _offsetElementCount{ 0 };
    U32           _elementCount{ 0 };
DEFINE_COMMAND_END(ClearBufferDataCommand);

}; //namespace GFX
}; //namespace Divide

#endif //DVD_GFX_COMMAND_INL_
