/*
Copyright (c) 2016 DIVIDE-Studio
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

#ifndef _SHADER_COMPUTE_QUEUE_H_
#define _SHADER_COMPUTE_QUEUE_H_

#include "ShaderProgramInfo.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Managers/Headers/FrameListenerManager.h"

namespace Divide {

namespace Time {
    class ProfileTimer;
};

class ResourceCache;
class PlatformContext;

class ShaderProgram;
class ShaderComputeQueue : public FrameListener {
public:
    struct ShaderQueueElement {
        ShaderProgramInfo* _shaderData = nullptr;
        ResourceDescriptor _shaderDescriptor;
    };

public:
    explicit ShaderComputeQueue(ResourceCache& cache);
    ~ShaderComputeQueue();

    // This is the main loop that steps through the queue and 
    // processes each entry
    void idle();
    // Push a process request at the front of the queue
    void addToQueueFront(const ShaderQueueElement& element);
    // Push a process request at the end of the queue
    void addToQueueBack(const ShaderQueueElement& element);
    // Process the first entry in the queue immediatelly
    // This is called in a loop in the 'update' call, but can be user 
    // called as well if the shader is needed immediatelly
    void stepQueue();

protected:
    bool frameStarted(const FrameEvent& evt) override;
    bool frameEnded(const FrameEvent& evt) override;

private:
    ResourceCache& _cache;

    Time::ProfileTimer& _queueComputeTimer;

    std::deque<ShaderQueueElement> _shaderComputeQueue;
    bool _shadersComputedThisFrame = false;
    U32  _totalShaderComputeCountThisFrame = 0;
    U32  _totalShaderComputeCount = 0;
};

}; //namespace Divide

#endif //_SHADER_COMPUTE_QUEUE_H_
