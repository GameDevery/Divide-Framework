/* Copyright (c) 2018 DIVIDE-Studio
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
#ifndef DVD_ECS_MANAGER_H_
#define DVD_ECS_MANAGER_H_

#include "ECS/Engine.h"
#include "Core/Headers/PlatformContextComponent.h"

namespace Divide {
    class ByteBuffer;
    class SceneGraphNode;
    class ECSManager final : public PlatformContextComponent {
        public:
            ECSManager(PlatformContext& context, ECS::ECSEngine& engine);

            [[nodiscard]] bool saveCache(const SceneGraphNode* sgn, ByteBuffer& outputBuffer) const;
            [[nodiscard]] bool loadCache(SceneGraphNode* sgn, ByteBuffer& inputBuffer) const;

            REFERENCE_R(ECS::ECSEngine, ecsEngine);
    };

    FWD_DECLARE_MANAGED_CLASS(ECSManager);

} //namespace Divide

#endif //DVD_ECS_MANAGER_H_
