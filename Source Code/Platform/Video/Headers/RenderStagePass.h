/*
Copyright (c) 2018 DIVIDE-Studio
Copyright (c) 2009 Ionut Cava

This file is part of DIVIDE Framework.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software
and associated documentation files (the "Software"), to deal in the Software
without restriction,
including without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#pragma once
#ifndef _RENDER_STAGE_PASS_H_
#define _RENDER_STAGE_PASS_H_

#include "Platform/Video/Headers/RenderAPIEnums.h"

namespace Divide {

struct RenderStagePass {
    typedef U8 PassIndex;

    RenderStagePass() noexcept : RenderStagePass(RenderStage::COUNT, RenderPassType::COUNT)
    {
    }

    RenderStagePass(RenderStage stage, RenderPassType passType) noexcept : RenderStagePass(stage, passType, 0)
    {
    }

    RenderStagePass(RenderStage stage, RenderPassType passType, U8 variant) noexcept
        : _stage(stage),
          _passType(passType),
          _variant(variant)
    {
    }

    U8 _variant = 0;
    RenderStage _stage = RenderStage::COUNT;
    RenderPassType _passType = RenderPassType::COUNT;

    inline PassIndex index() const {
        return index(_stage, _passType);
    }

    inline bool isDepthPass() const {
        return _stage == RenderStage::SHADOW ||
               _passType == RenderPassType::PRE_PASS;
    }

    constexpr static PassIndex count() {
        return static_cast<PassIndex>(to_base(RenderStage::COUNT) * to_base(RenderPassType::COUNT));
    }

    static PassIndex index(const RenderStage stage, const RenderPassType type) {
        return static_cast<PassIndex>(to_base(stage) + to_base(type) * to_base(RenderStage::COUNT));
    }

    static RenderStage stage(PassIndex index) {
        return static_cast<RenderStage>(index % to_base(RenderStage::COUNT));
    }

    static RenderPassType pass(PassIndex index) {
        return static_cast<RenderPassType>(index / to_base(RenderStage::COUNT));
    }

    static RenderStagePass stagePass(PassIndex index) {
        return RenderStagePass(RenderStagePass::stage(index), RenderStagePass::pass(index));
    }
};

inline bool operator==(const RenderStagePass& lhs, const RenderStagePass& rhs) {
    return lhs._stage == rhs._stage &&
           lhs._passType == rhs._passType;
}

inline bool operator!=(const RenderStagePass& lhs, const RenderStagePass& rhs) {
    return lhs._stage != rhs._stage ||
           lhs._passType != rhs._passType;
}

}; //namespace Divide

#endif //_RENDER_STAGE_PASS_H_