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
#ifndef DVD_CORE_KERNEL_H_
#define DVD_CORE_KERNEL_H_

#include "PlatformContext.h"
#include "LoopTimingData.h"
#include "Core/Headers/Application.h"
#include "Managers/Headers/FrameListenerManager.h"
#include "Platform/Input/Headers/InputAggregatorInterface.h"

namespace Divide {

class Scene;
class Editor;
class PXDevice;
class GFXDevice;
class SFXDevice;
class GUISplash;
class Application;
class ProjectManager;
class PlatformContext;
class SceneRenderState;
class RenderPassManager;

namespace Input
{
    struct MouseState;
    class InputInterface;
};

enum class RenderStage : U8;

struct FrameEvent;
struct DebugInterface;


namespace Attorney {
    class KernelApplication;
    class KernelWindowManager;
    class KernelDebugInterface;
};

namespace Time {
    class ProfileTimer;
};

/// The kernel is the main system that connects all of our various systems: windows, gfx, sfx, input, physics, timing, etc
class Kernel final : public Input::InputAggregatorInterface,
                     NonCopyable
{
    friend class Attorney::KernelApplication;
    friend class Attorney::KernelWindowManager;
    friend class Attorney::KernelDebugInterface;

   public:
    Kernel(I32 argc, char** argv, Application& parentApp);
    ~Kernel() override;

    /// Our main application rendering loop: Call input requests, physics calculations, pre-rendering, rendering,post-rendering etc
    void onLoop();
    /// Called after a swap-buffer call and before a clear-buffer call.
    /// In a GPU-bound application, the CPU will wait on the GPU to finish processing the frame so this should keep it busy (old-GLUT heritage)
    void idle(bool fast, U64 deltaTimeUSGame, U64 deltaTimeUSApp );
    
    /// Key pressed
    bool onKeyDown(const Input::KeyEvent& key) override;
    /// Key released
    bool onKeyUp(const Input::KeyEvent& key) override;
    /// Joystick axis change
    bool joystickAxisMoved(const Input::JoystickEvent& arg) override;
    /// Joystick direction change
    bool joystickPovMoved(const Input::JoystickEvent& arg) override;
    /// Joystick button pressed
    bool joystickButtonPressed(const Input::JoystickEvent& arg) override;
    /// Joystick button released
    bool joystickButtonReleased(const Input::JoystickEvent& arg) override;
    bool joystickBallMoved(const Input::JoystickEvent& arg) override;
    bool joystickAddRemove(const Input::JoystickEvent& arg) override;
    bool joystickRemap(const Input::JoystickEvent &arg) override;
    /// Mouse moved
    bool mouseMoved(const Input::MouseMoveEvent& arg) override;
    /// Mouse button pressed
    bool mouseButtonPressed(const Input::MouseButtonEvent& arg) override;
    /// Mouse button released
    bool mouseButtonReleased(const Input::MouseButtonEvent& arg) override;
    /// Generated by text events (e.g. for SDL: SDL_TEXTEDITING and SDL_TEXTINPUT)
    bool onTextEvent(const Input::TextEvent& arg) override;

    PROPERTY_RW(LoopTimingData, timingData);
    PROPERTY_RW(bool, keepAlive, true);
    PROPERTY_R(std::unique_ptr<ProjectManager>, projectManager);
    PROPERTY_R(std::unique_ptr<RenderPassManager>, renderPassManager);

    PROPERTY_R(FrameListenerManager, frameListenerMgr);
    PROPERTY_R(PlatformContext, platformContext);

    FORCE_INLINE FrameListenerManager& frameListenerMgr() noexcept { return _frameListenerMgr; }
    FORCE_INLINE PlatformContext& platformContext() noexcept { return _platformContext; }

    static size_t TotalThreadCount(TaskPoolType type) noexcept;

   private:
    ErrorCode initialize(const string& entryPoint);
    void warmup();
    void shutdown();
    void startSplashScreen();
    void stopSplashScreen();
    bool mainLoopScene(FrameEvent& evt);
    bool presentToScreen(FrameEvent& evt);

    void onWindowSizeChange(const SizeChangeParams& params);
    void onResolutionChange(const SizeChangeParams& params);

    void remapAbsolutePosition(Input::MouseEvent& eventInOut) const noexcept;

   private:
    enum class InputConsumerType : U8 {
        Editor,
        GUI,
        Scene,
        COUNT
    };

    std::array<InputAggregatorInterface*, to_base(InputConsumerType::COUNT)> _inputConsumers{};

    vector<Rect<I32>> _targetViewports{};

    Time::ProfileTimer& _appLoopTimerMain;
    Time::ProfileTimer& _appLoopTimerInternal;
    Time::ProfileTimer& _frameTimer;
    Time::ProfileTimer& _appIdleTimer;
    Time::ProfileTimer& _appScenePass;
    Time::ProfileTimer& _sceneUpdateTimer;
    Time::ProfileTimer& _sceneUpdateLoopTimer;
    Time::ProfileTimer& _cameraMgrTimer;
    Time::ProfileTimer& _flushToScreenTimer;
    Time::ProfileTimer& _preRenderTimer;
    Time::ProfileTimer& _postRenderTimer;
    vector<Time::ProfileTimer*> _renderTimer{};

    std::atomic_bool _splashScreenUpdating{};
    std::unique_ptr<GUISplash> _splashScreen;

    // Command line arguments
    I32 _argc;
    char** _argv;

    Rect<I32> _prevViewport = { -1, -1, -1, -1 };
    U8 _prevPlayerCount = 0u;
};

namespace Attorney {
    class KernelApplication {
        static ErrorCode initialize(Kernel* kernel, const string& entryPoint) {
            return kernel->initialize(entryPoint);
        }

        static void shutdown(Kernel* kernel) {
            kernel->shutdown();
        }

        static void onWindowSizeChange(Kernel* kernel, const SizeChangeParams& params) {
            kernel->onWindowSizeChange(params);
        }

        static void onResolutionChange(Kernel* kernel, const SizeChangeParams& params) {
            kernel->onResolutionChange(params);
        }

        static void warmup(Kernel* kernel) {
            kernel->warmup();
        }

        static void onLoop(Kernel* kernel) {
            kernel->onLoop();
        }

        friend class Divide::Application;
    };

    class KernelDebugInterface {
        static const LoopTimingData& timingData(const Kernel& kernel) noexcept {
            return kernel._timingData;
        }

        friend struct Divide::DebugInterface;
    };
};

};  // namespace Divide

#endif  //DVD_CORE_KERNEL_H_
