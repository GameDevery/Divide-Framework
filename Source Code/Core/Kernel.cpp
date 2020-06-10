#include "stdafx.h"

#include "config.h"

#include "Headers/Kernel.h"
#include "Headers/Configuration.h"
#include "Headers/EngineTaskPool.h"
#include "Headers/PlatformContext.h"
#include "Headers/XMLEntryData.h"

#include "Core/Debugging/Headers/DebugInterface.h"
#include "Core/Headers/ParamHandler.h"
#include "Core/Headers/StringHelper.h"
#include "Core/Networking/Headers/LocalClient.h"
#include "Core/Networking/Headers/Server.h"
#include "Core/Time/Headers/ApplicationTimer.h"
#include "Core/Time/Headers/ProfileTimer.h"
#include "Editor/Headers/Editor.h"
#include "GUI/Headers/GUI.h"
#include "GUI/Headers/GUIConsole.h"
#include "GUI/Headers/GUISplash.h"
#include "Managers/Headers/FrameListenerManager.h"
#include "Managers/Headers/RenderPassManager.h"
#include "Managers/Headers/SceneManager.h"
#include "Physics/Headers/PXDevice.h"
#include "Platform/Audio/Headers/SFXDevice.h"
#include "Platform/File/Headers/FileWatcherManager.h"
#include "Platform/Headers/SDLEventManager.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Rendering/Camera/Headers/FreeFlyCamera.h"
#include "Rendering/Headers/Renderer.h"
#include "Rendering/PostFX/Headers/PostFX.h"
#include "Resources/Headers/ResourceCache.h"
#include "Scripting/Headers/Script.h"
#include "Utility/Headers/XMLParser.h"

namespace Divide {

namespace {
    constexpr F32 g_PhysicsSimSpeedFactor = 1.0f;
    constexpr U32 g_backupThreadPoolSize = 2u;
    constexpr U32 g_printTimerBase = 15u;

    U32 g_printTimer = g_printTimerBase;

};

Kernel::Kernel(const I32 argc, char** argv, Application& parentApp)
    : _platformContext(PlatformContext(parentApp, *this)),
      _appLoopTimer(Time::ADD_TIMER("Main Loop Timer")),
      _frameTimer(Time::ADD_TIMER("Total Frame Timer")),
      _appIdleTimer(Time::ADD_TIMER("Loop Idle Timer")),
      _appScenePass(Time::ADD_TIMER("Loop Scene Pass Timer")),
      _physicsUpdateTimer(Time::ADD_TIMER("Physics Update Timer")),
      _physicsProcessTimer(Time::ADD_TIMER("Physics Process Timer")),
      _sceneUpdateTimer(Time::ADD_TIMER("Scene Update Timer")),
      _sceneUpdateLoopTimer(Time::ADD_TIMER("Scene Update Loop timer")),
      _cameraMgrTimer(Time::ADD_TIMER("Camera Manager Update Timer")),
      _flushToScreenTimer(Time::ADD_TIMER("Flush To Screen Timer")),
      _preRenderTimer(Time::ADD_TIMER("Pre-render Timer")),
      _postRenderTimer(Time::ADD_TIMER("Post-render Timer")),
      _argc(argc),
      _argv(argv)
{
    std::atomic_init(&_splashScreenUpdating, false);
    _appLoopTimer.addChildTimer(_appIdleTimer);
    _appLoopTimer.addChildTimer(_frameTimer);
    _frameTimer.addChildTimer(_appScenePass);
    _appScenePass.addChildTimer(_cameraMgrTimer);
    _appScenePass.addChildTimer(_physicsUpdateTimer);
    _appScenePass.addChildTimer(_physicsProcessTimer);
    _appScenePass.addChildTimer(_sceneUpdateTimer);
    _appScenePass.addChildTimer(_flushToScreenTimer);
    _flushToScreenTimer.addChildTimer(_preRenderTimer);
    _flushToScreenTimer.addChildTimer(_postRenderTimer);
    _sceneUpdateTimer.addChildTimer(_sceneUpdateLoopTimer);

    _sceneManager = MemoryManager_NEW SceneManager(*this); // Scene Manager
    _resourceCache = MemoryManager_NEW ResourceCache(_platformContext);
    _renderPassManager = MemoryManager_NEW RenderPassManager(*this, _platformContext.gfx());
}

Kernel::~Kernel()
{
    DIVIDE_ASSERT(sceneManager() == nullptr && resourceCache() == nullptr && renderPassManager() == nullptr,
                  "Kernel destructor: not all resources have been released properly!");
}

void Kernel::startSplashScreen() {
    bool expected = false;
    if (!_splashScreenUpdating.compare_exchange_strong(expected, true)) {
        return;
    }

    DisplayWindow& window = _platformContext.mainWindow();
    window.changeType(WindowType::WINDOW);
    window.decorated(false);
    WAIT_FOR_CONDITION(window.setDimensions(_platformContext.config().runtime.splashScreenSize));

    window.centerWindowPosition();
    window.hidden(false);
    SDLEventManager::pollEvents();

    GUISplash splash(resourceCache(), "divideLogo.jpg", _platformContext.config().runtime.splashScreenSize);

    // Load and render the splash screen
    _splashTask = CreateTask(_platformContext,
        [this, &splash](const Task& /*task*/) {
        U64 previousTimeUS = 0;
        while (_splashScreenUpdating) {
            const U64 deltaTimeUS = Time::App::ElapsedMicroseconds() - previousTimeUS;
            previousTimeUS += deltaTimeUS;
            _platformContext.beginFrame(PlatformContext::SystemComponentType::GFXDevice);
            splash.render(_platformContext.gfx(), deltaTimeUS);
            _platformContext.endFrame(PlatformContext::SystemComponentType::GFXDevice);
            std::this_thread::sleep_for(std::chrono::milliseconds(15));

            break;
        }
    });
    Start(*_splashTask, TaskPriority::REALTIME/*HIGH*/);

    window.swapBuffers(false);
}

void Kernel::stopSplashScreen() {
    DisplayWindow& window = _platformContext.mainWindow();
    window.swapBuffers(true);
    const vec2<U16> previousDimensions = window.getPreviousDimensions();
    _splashScreenUpdating = false;
    Wait(*_splashTask);

    window.changeToPreviousType();
    window.decorated(true);
    WAIT_FOR_CONDITION(window.setDimensions(previousDimensions));
    window.setPosition(vec2<I32>(-1));
    if (window.type() == WindowType::WINDOW && _platformContext.config().runtime.maximizeOnStart) {
        window.maximized(true);
    }
    SDLEventManager::pollEvents();
}

void Kernel::idle(const bool fast) {
    OPTICK_EVENT();

    _platformContext.idle();

    if_constexpr (!Config::Build::IS_SHIPPING_BUILD) {
        if (!fast) {
            FileWatcherManager::idle();
            Locale::idle();
        }
    }
    _sceneManager->idle();
    Script::idle();

    if (--g_printTimer == 0) {
        Console::printAll();
        g_printTimer = g_printTimerBase;
    }
    frameListenerMgr().idle();

    bool freezeLoopTime = _platformContext.paramHandler().getParam(_ID("freezeLoopTime"), false);

    if_constexpr(Config::Build::ENABLE_EDITOR) {
        freezeLoopTime = _platformContext.editor().simulationPauseRequested() || freezeLoopTime;
    }

    if (_timingData.freezeTime(freezeLoopTime)) {
        _platformContext.app().mainLoopPaused(freezeLoopTime);
    }
}

void Kernel::onLoop() {
    if (!_timingData.keepAlive()) {
        // exiting the rendering loop will return us to the last control point
        _platformContext.app().mainLoopActive(false);
        sceneManager()->saveActiveScene(true, false);
        return;
    }

    // Update internal timer
    _platformContext.app().timer().update();
    {
        Time::ScopedTimer timer(_appLoopTimer);
   
        // Update time at every render loop
        _timingData.update(Time::Game::ElapsedMicroseconds());
        FrameEvent evt = {};

        // Restore GPU to default state: clear buffers and set default render state
        _platformContext.beginFrame();
        {
            Time::ScopedTimer timer3(_frameTimer);
            // Launch the FRAME_STARTED event
            _timingData.keepAlive(frameListenerMgr().createAndProcessEvent(Time::Game::ElapsedMicroseconds(), FrameEventType::FRAME_EVENT_STARTED, evt));

            const U64 deltaTimeUSApp = _timingData.currentTimeDeltaUS();
            const U64 deltaTimeUSReal = _timingData.timeDeltaUS();

            const U64 deltaTimeUS = _timingData.freezeLoopTime() 
                                               ? 0ULL
                                               : Time::SecondsToMicroseconds(1) / TICKS_PER_SECOND;

            // Process the current frame
            _timingData.keepAlive(_timingData.keepAlive() && mainLoopScene(evt, deltaTimeUS, deltaTimeUSReal, deltaTimeUSApp));

            // Launch the FRAME_PROCESS event (a.k.a. the frame processing has ended event)
            _timingData.keepAlive(_timingData.keepAlive() && frameListenerMgr().createAndProcessEvent(Time::App::ElapsedMicroseconds(), FrameEventType::FRAME_EVENT_PROCESS, evt));
        }
        _platformContext.endFrame();

        // Launch the FRAME_ENDED event (buffers have been swapped)

        _timingData.keepAlive(_timingData.keepAlive() && frameListenerMgr().createAndProcessEvent(Time::App::ElapsedMicroseconds(), FrameEventType::FRAME_EVENT_ENDED, evt));

        _timingData.keepAlive(_timingData.keepAlive() && !_platformContext.app().ShutdownRequested());
    
        const ErrorCode err = _platformContext.app().errorCode();

        if (err != ErrorCode::NO_ERR) {
            Console::errorfn("Error detected: [ %s ]", getErrorCodeName(err));
            _timingData.keepAlive(false);
        }
    }

    const U32 frameCount = _platformContext.gfx().getFrameCount();
    // Should equate to approximately once every 10 seconds
    if (platformContext().debug().enabled() && frameCount % (Config::TARGET_FRAME_RATE * Time::Seconds(10)) == 0) {
        Console::printfn(platformContext().debug().output().c_str());
    }

    if (frameCount % (Config::TARGET_FRAME_RATE / 8) == 0u) {
        _platformContext.gui().modifyText("ProfileData", platformContext().debug().output(), true);
    }

    if (frameCount % 4 == 0u) {
        F32 fps = 0.f, frameTime = 0.f;
        DisplayWindow& window = _platformContext.mainWindow();
        static stringImpl originalTitle = window.title();
        _platformContext.app().timer().getFrameRateAndTime(fps, frameTime);
        window.title("%s - %5.2f FPS - %3.2f ms - FrameIndex: %d", originalTitle, fps, frameTime, platformContext().gfx().getFrameCount());
    }

    // Cap FPS
    const I16 frameLimit = _platformContext.config().runtime.frameRateLimit;
    const F32 deltaMilliseconds = Time::MicrosecondsToMilliseconds<F32>(_timingData.timeDeltaUS());
    const F32 targetFrameTime = 1000.0f / frameLimit;

    if (deltaMilliseconds < targetFrameTime) {
        {
            Time::ScopedTimer timer2(_appIdleTimer);
            idle(true);
        }

        if (frameLimit > 0) {
            //Sleep the remaining frame time 
            std::this_thread::sleep_for(std::chrono::milliseconds(to_I32(std::floorf(targetFrameTime - deltaMilliseconds))));
        }
    }
}

bool Kernel::mainLoopScene(FrameEvent& evt,
                           const U64 deltaTimeUS,     //Frame rate independent deltaTime. Can be paused. (e.g. used by scene updates)
                           const U64 realDeltaTimeUS, //Frame rate dependent deltaTime. Can be paused. (e.g. used by physics)
                           const U64 appDeltaTimeUS)  //Real app delta time between frames. Can't be paused (e.g. used by editor)
{
    OPTICK_EVENT();

    Time::ScopedTimer timer(_appScenePass);
    {
        Time::ScopedTimer timer2(_cameraMgrTimer);
        // Update cameras 
        // ToDo: add a speed slider in the editor -Ionut
        Camera::update(_timingData.freezeLoopTime() ? (appDeltaTimeUS / 2) : deltaTimeUS);
    }

    if (_platformContext.mainWindow().minimized()) {
        idle(false);
        SDLEventManager::pollEvents();
        return true;
    }

    {
        Time::ScopedTimer timer2(_physicsProcessTimer);
        // Process physics
        _platformContext.pfx().process(realDeltaTimeUS);
    }
    {
        Time::ScopedTimer timer2(_sceneUpdateTimer);

        const U8 playerCount = _sceneManager->getActivePlayerCount();

        U8 loopCount = 0;
        while (_timingData.runUpdateLoop()) {

            if (loopCount == 0) {
                _sceneUpdateLoopTimer.start();
            }
            _sceneManager->onStartUpdateLoop(loopCount);

            _sceneManager->processGUI(deltaTimeUS);

            // Flush any pending threaded callbacks
            for (U32 i = 0; i < to_U32(TaskPoolType::COUNT); ++i) {
                _platformContext.taskPool(static_cast<TaskPoolType>(i)).flushCallbackQueue();
            }

            // Update scene based on input
            for (U8 i = 0; i < playerCount; ++i) {
                _sceneManager->processInput(i, deltaTimeUS);
            }
            // process all scene events
            _sceneManager->processTasks(deltaTimeUS);
            // Update the scene state based on current time (e.g. animation matrices)
            _sceneManager->updateSceneState(deltaTimeUS);
            // Update visual effect timers as well
            _platformContext.gfx().getRenderer().postFX().update(deltaTimeUS);

            if (loopCount == 0) {
                _sceneUpdateLoopTimer.stop();
            }

            _timingData.endUpdateLoop(deltaTimeUS, true);
            ++loopCount;
        }  // while
    }

    const U32 frameCount = _platformContext.gfx().getFrameCount();

    if (frameCount % (Config::TARGET_FRAME_RATE / Config::Networking::NETWORK_SEND_FREQUENCY_HZ) == 0) {
        U32 retryCount = 0;
        while (!Attorney::SceneManagerKernel::networkUpdate(_sceneManager, frameCount)) {
            if (retryCount++ > Config::Networking::NETWORK_SEND_RETRY_COUNT) {
                break;
            }
        }
    }

    D64 interpolationFactor = 1.0;
    if (!_timingData.freezeLoopTime()) {
        interpolationFactor = static_cast<D64>(_timingData.currentTimeUS() + deltaTimeUS - _timingData.nextGameTickUS()) / deltaTimeUS;
        CLAMP_01(interpolationFactor);
    }

    GFXDevice::setFrameInterpolationFactor(interpolationFactor);
    
    // Update windows and get input events
    SDLEventManager::pollEvents();

    WindowManager& winManager = _platformContext.app().windowManager();
    winManager.update(appDeltaTimeUS);
    {
        Time::ScopedTimer timer3(_physicsUpdateTimer);
        // Update physics
        _platformContext.pfx().update(realDeltaTimeUS);
    }

    // Update the graphical user interface
    _platformContext.gui().update(deltaTimeUS);

    if_constexpr(Config::Build::ENABLE_EDITOR) {
        _platformContext.editor().update(appDeltaTimeUS);
    }

    return presentToScreen(evt, deltaTimeUS);
}

void computeViewports(const Rect<I32>& mainViewport, vectorEASTL<Rect<I32>>& targetViewports, U8 count) {
    
    assert(count > 0);
    const I32 xOffset = mainViewport.x;
    const I32 yOffset = mainViewport.y;
    const I32 width = mainViewport.z;
    const I32 height = mainViewport.w;

    targetViewports.resize(0);
    if (count == 1) { //Single Player
        targetViewports.push_back(mainViewport);
        return;
    } else if (count == 2) { //Split Screen
        I32 halfHeight = height / 2;
        targetViewports.emplace_back(xOffset, halfHeight + yOffset, width, halfHeight);
        targetViewports.emplace_back(xOffset, 0 + yOffset,          width, halfHeight);
        return;
    }

    // Basic idea (X - viewport):
    // Odd # of players | Even # of players
    // X X              |      X X
    //  X               |      X X
    //                  |
    // X X X            |     X X X 
    //  X X             |     X X X
    // etc
    // Always try to match last row with previous one
    // If they match, move the first viewport from the last row
    // to the previous row and add a new entry to the end of the
    // current row

    using ViewportRow = vectorEASTL<Rect<I32>>;
    using ViewportRows = vectorEASTL<ViewportRow>;
    ViewportRows rows;

    // Allocates storage for a N x N matrix of viewports that will hold numViewports
    // Returns N;
    const auto resizeViewportContainer = [&rows](U32 numViewports) {
        //Try to fit all viewports into an appropriately sized matrix.
        //If the number of resulting rows is too large, drop empty rows.
        //If the last row has an odd number of elements, center them later.
        const U8 matrixSize = to_U8(minSquareMatrixSize(numViewports));
        rows.resize(matrixSize);
        std::for_each(std::begin(rows), std::end(rows), [matrixSize](ViewportRow& row) { row.resize(matrixSize); });

        return matrixSize;
    };

    // Remove extra rows and columns, if any
    const U8 columnCount = resizeViewportContainer(count);
    const U8 extraColumns = (columnCount * columnCount) - count;
    const U8 extraRows = extraColumns / columnCount;
    for (U8 i = 0; i < extraRows; ++i) {
        rows.pop_back();
    }
    const U8 columnsToRemove = extraColumns - (extraRows * columnCount);
    for (U8 i = 0; i < columnsToRemove; ++i) {
        rows.back().pop_back();
    }

    const U8 rowCount = to_U8(rows.size());

    // Calculate and set viewport dimensions
    // The number of columns is valid for the width;
    const I32 playerWidth = width / columnCount;
    // The number of rows is valid for the height;
    const I32 playerHeight = height / to_I32(rowCount);

    for (U8 i = 0; i < rowCount; ++i) {
        ViewportRow& row = rows[i];
        const I32 playerYOffset = playerHeight * (rowCount - i - 1);
        for (U8 j = 0; j < to_U8(row.size()); ++j) {
            const I32 playerXOffset = playerWidth * j;
            row[j].set(playerXOffset, playerYOffset, playerWidth, playerHeight);
        }
    }

    //Slide the last row to center it
    if (extraColumns > 0) {
        ViewportRow& lastRow = rows.back();
        const I32 screenMidPoint = width / 2;
        const I32 rowMidPoint = to_I32((lastRow.size() * playerWidth) / 2);
        const I32 slideFactor = screenMidPoint - rowMidPoint;
        for (Rect<I32>& viewport : lastRow) {
            viewport.x += slideFactor;
        }
    }

    // Update the system viewports
    for (const ViewportRow& row : rows) {
        for (const Rect<I32>& viewport : row) {
            targetViewports.push_back(viewport);
        }
    }
}

Time::ProfileTimer& getTimer(Time::ProfileTimer& parentTimer, vectorEASTL<Time::ProfileTimer*>& timers, U8 index, const char* name) {
    while (timers.size() < to_size(index) + 1) {
        timers.push_back(&Time::ADD_TIMER(Util::StringFormat("%s %d", name, timers.size()).c_str()));
        parentTimer.addChildTimer(*timers.back());
    }

    return *timers[index];
}

bool Kernel::presentToScreen(FrameEvent& evt, const U64 deltaTimeUS) {
    OPTICK_EVENT();

    Time::ScopedTimer time(_flushToScreenTimer);

    {
        Time::ScopedTimer time1(_preRenderTimer);
        if (!frameListenerMgr().createAndProcessEvent(Time::App::ElapsedMicroseconds(), FrameEventType::FRAME_PRERENDER_START, evt)) {
            return false;
        }

        // perform time-sensitive shader tasks
        if (!ShaderProgram::updateAll()) {
            return false;
        }

        if (!frameListenerMgr().createAndProcessEvent(Time::App::ElapsedMicroseconds(), FrameEventType::FRAME_PRERENDER_END, evt)) {
            return false;
        }
    }

    const U8 playerCount = _sceneManager->getActivePlayerCount();
    const bool editorRunning = Config::Build::ENABLE_EDITOR && _platformContext.editor().running();

    const Rect<I32> mainViewport = _platformContext.mainWindow().renderingViewport();
    
    if (_prevViewport != mainViewport || _prevPlayerCount != playerCount) {
        computeViewports(mainViewport, _targetViewports, playerCount);
        _prevViewport.set(mainViewport);
        _prevPlayerCount = playerCount;
    }

    if (editorRunning) {
        computeViewports(_platformContext.editor().targetViewport(), _editorViewports, playerCount);
    }

    RenderPassManager::RenderParams renderParams = {};
    renderParams._editorRunning = editorRunning;
    renderParams._sceneRenderState = &_sceneManager->getActiveScene().renderState();

    for (U8 i = 0; i < playerCount; ++i) {
        if (!frameListenerMgr().createAndProcessEvent(Time::App::ElapsedMicroseconds(), FrameEventType::FRAME_SCENERENDER_START, evt)) {
            return false;
        }

        renderParams._targetViewport = editorRunning ? _editorViewports[i] : _targetViewports[i];

        Attorney::SceneManagerKernel::currentPlayerPass(_sceneManager, i);
        {
            Time::ProfileTimer& timer = getTimer(_flushToScreenTimer, _renderTimer, i, "Render Timer");
            renderParams._parentTimer = &timer;
            Time::ScopedTimer time2(timer);
            _renderPassManager->render(renderParams);
        }

        if (!frameListenerMgr().createAndProcessEvent(Time::App::ElapsedMicroseconds(), FrameEventType::FRAME_SCENERENDER_END, evt)) {
            return false;
        }
    }

    {
        Time::ScopedTimer time4(_postRenderTimer);
        if(!frameListenerMgr().createAndProcessEvent(Time::App::ElapsedMicroseconds(), FrameEventType::FRAME_POSTRENDER_START, evt)) {
            return false;
        }

        if (!frameListenerMgr().createAndProcessEvent(Time::App::ElapsedMicroseconds(), FrameEventType::FRAME_POSTRENDER_END, evt)) {
            return false;
        }
    }

    for (U32 i = playerCount; i < to_U32(_renderTimer.size()); ++i) {
        Time::ProfileTimer::removeTimer(*_renderTimer[i]);
        _renderTimer.erase(std::begin(_renderTimer) + i);
    }

    return true;
}

// The first loops compiles all the visible data, so do not render the first couple of frames
void Kernel::warmup() {
    Console::printfn(Locale::get(_ID("START_RENDER_LOOP")));

    _platformContext.paramHandler().setParam(_ID("freezeLoopTime"), true);
    onLoop();
    _platformContext.paramHandler().setParam(_ID("freezeLoopTime"), false);

    Attorney::SceneManagerKernel::initPostLoadState(_sceneManager);

    _timingData.update(Time::App::ElapsedMicroseconds());

    stopSplashScreen();
}

ErrorCode Kernel::initialize(const stringImpl& entryPoint) {
    const SysInfo& systemInfo = const_sysInfo();
    if (Config::REQUIRED_RAM_SIZE > systemInfo._availableRam) {
        return ErrorCode::NOT_ENOUGH_RAM;
    }

    // Don't log parameter requests
    _platformContext.paramHandler().setDebugOutput(false);
    // Load info from XML files
    XMLEntryData& entryData = _platformContext.entryData();
    Configuration& config = _platformContext.config();
    XML::loadFromXML(entryData, entryPoint.c_str());
    XML::loadFromXML(config, (entryData.scriptLocation + "/config.xml").c_str());

    if (Util::FindCommandLineArgument(_argc, _argv, "disableRenderAPIDebugging")) {
        config.debug.enableRenderAPIDebugging = false;
    }

    if (config.runtime.targetRenderingAPI >= to_U8(RenderAPI::COUNT)) {
        config.runtime.targetRenderingAPI = to_U8(RenderAPI::OpenGL);
    }

    DIVIDE_ASSERT(g_backupThreadPoolSize >= 2u, "Backup thread pool needs at least 2 threads to handle background tasks without issues!");

    I32 threadCount = config.runtime.maxWorkerThreads;
    if (config.runtime.maxWorkerThreads < 0) {
        threadCount = std::max(HardwareThreadCount(), to_U32(RenderStage::COUNT) + g_backupThreadPoolSize);
    }
    totalThreadCount(threadCount);

    // Create mem log file
    const Str256& mem = config.debug.memFile.c_str();
    _platformContext.app().setMemoryLogFile(mem.compare("none") == 0 ? "mem.log" : mem);
    _platformContext.pfx().setAPI(PXDevice::PhysicsAPI::PhysX);
    _platformContext.sfx().setAPI(SFXDevice::AudioAPI::SDL);

    ASIO::SET_LOG_FUNCTION([](std::string_view msg, bool is_error) {
        is_error ? Console::errorfn(stringImpl(msg).c_str()) : Console::printfn(stringImpl(msg).c_str());
    });

    _platformContext.server().init(static_cast<Divide::U16>(443), "127.0.0.1", true);

    if (!_platformContext.client().connect(entryData.serverAddress, 443)) {
        _platformContext.client().connect("127.0.0.1", 443);
    }

    Paths::updatePaths(_platformContext);

    Locale::changeLanguage(config.language.c_str());
    ECS::Initialize();

    Console::printfn(Locale::get(_ID("START_RENDER_INTERFACE")));

    const RenderAPI renderingAPI = static_cast<RenderAPI>(config.runtime.targetRenderingAPI);

    WindowManager& winManager = _platformContext.app().windowManager();
    ErrorCode initError = winManager.init(_platformContext,
                                          renderingAPI,
                                          vec2<I16>(-1),
                                          config.runtime.windowSize,
                                          static_cast<WindowMode>(config.runtime.windowedMode),
                                          config.runtime.targetDisplay);

    if (initError != ErrorCode::NO_ERR) {
        return initError;
    }

    Camera::initPool();

    initError = _platformContext.gfx().initRenderingAPI(_argc, _argv, renderingAPI, config.runtime.resolution);

    // If we could not initialize the graphics device, exit
    if (initError != ErrorCode::NO_ERR) {
        return initError;
    }
    std::atomic_size_t threadCounter = totalThreadCount();
    assert(threadCounter.load() > g_backupThreadPoolSize);

    if (!_platformContext.taskPool(TaskPoolType::HIGH_PRIORITY).init(to_U32(totalThreadCount()) - g_backupThreadPoolSize,
                                                                     TaskPool::TaskPoolType::TYPE_BLOCKING,
                                                                     [this, &threadCounter](const std::thread::id& threadID) {
                                                                         Attorney::PlatformContextKernel::onThreadCreated(platformContext(), threadID);
                                                                         threadCounter.fetch_sub(1);
                                                                     },
                                                                     "DIVIDE_WORKER_THREAD_"))
    {
        return ErrorCode::CPU_NOT_SUPPORTED;
    }

    if (!_platformContext.taskPool(TaskPoolType::LOW_PRIORITY).init(g_backupThreadPoolSize,
                                                                    TaskPool::TaskPoolType::TYPE_BLOCKING,
                                                                    [this, &threadCounter](const std::thread::id& threadID) {
                                                                        Attorney::PlatformContextKernel::onThreadCreated(platformContext(), threadID);
                                                                        threadCounter.fetch_sub(1);
                                                                    },
                                                                    "DIVIDE_BACKUP_THREAD_"))
    {
        return ErrorCode::CPU_NOT_SUPPORTED;
    }

    WAIT_FOR_CONDITION(threadCounter.load() == 0);

    initError = _platformContext.gfx().postInitRenderingAPI();
    // If we could not initialize the graphics device, exit
    if (initError != ErrorCode::NO_ERR) {
        return initError;
    }
    winManager.postInit();

    // Add our needed app-wide render passes. RenderPassManager is responsible for deleting these!
    _renderPassManager->addRenderPass("shadowPass",     0, RenderStage::SHADOW);
    _renderPassManager->addRenderPass("reflectionPass", 1, RenderStage::REFLECTION, { 0 });
    _renderPassManager->addRenderPass("refractionPass", 2, RenderStage::REFRACTION, { 0 });
    _renderPassManager->addRenderPass("displayStage",   3, RenderStage::DISPLAY, { 1, 2}, true);

    Console::printfn(Locale::get(_ID("SCENE_ADD_DEFAULT_CAMERA")));

    Script::onStartup();
    SceneManager::onStartup();
    Attorney::ShaderProgramKernel::useShaderTextCache(config.debug.useShaderTextCache);
    Attorney::ShaderProgramKernel::useShaderBinaryCache(config.debug.useShaderBinaryCache);

    winManager.mainWindow()->addEventListener(WindowEvent::LOST_FOCUS, [this](const DisplayWindow::WindowEventArgs& args) {
        _sceneManager->onLostFocus();
        return true;
    });
    winManager.mainWindow()->addEventListener(WindowEvent::GAINED_FOCUS, [this](const DisplayWindow::WindowEventArgs& args) {
        _sceneManager->onGainFocus();
        return true;
    });

    // Initialize GUI with our current resolution
    _platformContext.gui().init(_platformContext, resourceCache());
    startSplashScreen();

    Console::printfn(Locale::get(_ID("START_SOUND_INTERFACE")));
    initError = _platformContext.sfx().initAudioAPI(_platformContext);
    if (initError != ErrorCode::NO_ERR) {
        return initError;
    }

    Console::printfn(Locale::get(_ID("START_PHYSICS_INTERFACE")));
    initError = _platformContext.pfx().initPhysicsAPI(Config::TARGET_FRAME_RATE, g_PhysicsSimSpeedFactor);
    if (initError != ErrorCode::NO_ERR) {
        return initError;
    }

    _platformContext.gui().addText("ProfileData",                                 // Unique ID
                                    RelativePosition2D(RelativeValue(0.75f, 0.0f),
                                                       RelativeValue(0.2f, 0.0f)), // Position
                                    Font::DROID_SERIF_BOLD,                        // Font
                                    UColour4(255,  50, 0, 255),                    // Colour
                                    "PROFILE DATA",                                // Text
                                    true,                                          // Multiline
                                    12);                                           // Font size

    ShadowMap::initShadowMaps(_platformContext.gfx());
    _sceneManager->init(_platformContext, resourceCache());

    if (!_sceneManager->switchScene(entryData.startupScene.c_str(), true, {0, 0, config.runtime.resolution.width, config.runtime.resolution.height}, false)) {
        Console::errorfn(Locale::get(_ID("ERROR_SCENE_LOAD")), entryData.startupScene.c_str());
        return ErrorCode::MISSING_SCENE_DATA;
    }

    if (!_sceneManager->checkLoadFlag()) {
        Console::errorfn(Locale::get(_ID("ERROR_SCENE_LOAD_NOT_CALLED")),
                         entryData.startupScene.c_str());
        return ErrorCode::MISSING_SCENE_LOAD_CALL;
    }

    _renderPassManager->postInit();

    if_constexpr (Config::Build::ENABLE_EDITOR) {
        if (!_platformContext.editor().init(config.runtime.resolution)) {
            return ErrorCode::EDITOR_INIT_ERROR;
        }
        _sceneManager->addSelectionCallback([&](PlayerIndex idx, const vectorEASTL<SceneGraphNode*>& nodes) {
            _platformContext.editor().selectionChangeCallback(idx, nodes);
        });
    }

    Console::printfn(Locale::get(_ID("INITIAL_DATA_LOADED")));

    return initError;
}

void Kernel::shutdown() {
    Console::printfn(Locale::get(_ID("STOP_KERNEL")));

    _platformContext.config().save();

    for (U32 i = 0; i < to_U32(TaskPoolType::COUNT); ++i) {
        WaitForAllTasks(_platformContext.taskPool(static_cast<TaskPoolType>(i)), true, true);
    }
    
    if_constexpr (Config::Build::ENABLE_EDITOR) {
        _platformContext.editor().toggle(false);
    }
    SceneManager::onShutdown();
    Script::onShutdown();
    MemoryManager::SAFE_DELETE(_sceneManager);
    ECS::Terminate();

    ShadowMap::destroyShadowMaps(_platformContext.gfx());
    MemoryManager::SAFE_DELETE(_renderPassManager);

    Camera::destroyPool();
    _platformContext.terminate();
    resourceCache()->clear();
    MemoryManager::SAFE_DELETE(_resourceCache);

    Console::printfn(Locale::get(_ID("STOP_ENGINE_OK")));
}

bool Kernel::onSizeChange(const SizeChangeParams& params) {

    const bool ret = Attorney::GFXDeviceKernel::onSizeChange(_platformContext.gfx(), params);

    if (!_splashScreenUpdating) {
        _platformContext.gui().onSizeChange(params);
    }

    if_constexpr (Config::Build::ENABLE_EDITOR) {
        _platformContext.editor().onSizeChange(params);
    }

    if (!params.isWindowResize) {
        _sceneManager->onSizeChange(params);
    }

    return ret;
}

///--------------------------Input Management-------------------------------------///
bool Kernel::onKeyDown(const Input::KeyEvent& key) {
    if_constexpr (Config::Build::ENABLE_EDITOR) {

        Editor& editor = _platformContext.editor();
        if (!_sceneManager->wantsKeyboard() && editor.onKeyDown(key)) {
            return true;
        }
    }

    if (!_platformContext.gui().onKeyDown(key)) {
        return _sceneManager->onKeyDown(key);
    }
    return true;  //< InputInterface needs to know when this is completed
}

bool Kernel::onKeyUp(const Input::KeyEvent& key) {
    if_constexpr (Config::Build::ENABLE_EDITOR) {
        Editor& editor = _platformContext.editor();
        if (!_sceneManager->wantsKeyboard() && editor.onKeyUp(key)) {
            return true;
        }
    }

    if (!_platformContext.gui().onKeyUp(key)) {
        return _sceneManager->onKeyUp(key);
    }
    // InputInterface needs to know when this is completed
    return false;
}

vec2<I32> Kernel::remapMouseCoords(const vec2<I32>& absPositionIn, bool& remappedOut) const noexcept {
    remappedOut = false;
    if_constexpr(Config::Build::ENABLE_EDITOR) {
        const Editor& editor = _platformContext.editor();
        if (editor.running() && editor.scenePreviewFocused()) {
            const Rect<I32>& sceneRect = editor.scenePreviewRect(false);
            if (sceneRect.contains(absPositionIn)) {
                remappedOut = true;
                const Rect<I32>& viewport = _platformContext.gfx().getCurrentViewport();
                return COORD_REMAP(absPositionIn, sceneRect, viewport);
            }
        }
    }

    return absPositionIn;
}

bool Kernel::mouseMoved(const Input::MouseMoveEvent& arg) {
    if_constexpr(Config::Build::ENABLE_EDITOR) {
        Editor& editor = _platformContext.editor();
        if (!_sceneManager->wantsMouse() && editor.mouseMoved(arg)) {
            _sceneManager->mouseMovedExternally(arg);
            return true;
        }
    }

    Input::MouseMoveEvent remapArg = arg;
    //Remap coords in case we are using the Editor's scene view
    if_constexpr(Config::Build::ENABLE_EDITOR) {
        bool remapped = false;
        const vec2<I32> newPos = remapMouseCoords(arg.absolutePos(), remapped);
        if (remapped) {
            Input::Attorney::MouseEventKernel::absolutePos(remapArg, newPos);
        }
    }

    if (!_platformContext.gui().mouseMoved(remapArg)) {
        return _sceneManager->mouseMoved(remapArg);
    } else {
        _sceneManager->mouseMovedExternally(arg);
    }
    
    // InputInterface needs to know when this is completed
    return false;
}

bool Kernel::mouseButtonPressed(const Input::MouseButtonEvent& arg) {
    
    if_constexpr (Config::Build::ENABLE_EDITOR) {
        Editor& editor = _platformContext.editor();
        if (!_sceneManager->wantsMouse() && editor.mouseButtonPressed(arg)) {
            return true;
        }
    }

    Input::MouseButtonEvent remapArg = arg;
    if_constexpr(Config::Build::ENABLE_EDITOR) {
        bool remapped = false;
        const vec2<I32> newPos = remapMouseCoords(arg.absPosition(), remapped);
        if (remapped) {
            Input::Attorney::MouseEventKernel::absolutePos(remapArg, newPos);
        }
    }

    if (!_platformContext.gui().mouseButtonPressed(remapArg)) {
        return _sceneManager->mouseButtonPressed(remapArg);
    }
    
    // InputInterface needs to know when this is completed
    return false;
}

bool Kernel::mouseButtonReleased(const Input::MouseButtonEvent& arg) {
    
    if_constexpr (Config::Build::ENABLE_EDITOR) {
        Editor& editor = _platformContext.editor();
        if (!_sceneManager->wantsMouse() && editor.mouseButtonReleased(arg)) {
            return true;
        }
    }

    Input::MouseButtonEvent remapArg = arg;
    if_constexpr(Config::Build::ENABLE_EDITOR) {
        bool remapped = false;
        const vec2<I32> newPos = remapMouseCoords(arg.absPosition(), remapped);
        if (remapped) {
            Input::Attorney::MouseEventKernel::absolutePos(remapArg, newPos);
        }
    }

    if (!_platformContext.gui().mouseButtonReleased(remapArg)) {
        return _sceneManager->mouseButtonReleased(remapArg);
    }

    // InputInterface needs to know when this is completed
    return false;
}

bool Kernel::joystickAxisMoved(const Input::JoystickEvent& arg) {
    if (!_platformContext.gui().joystickAxisMoved(arg)) {
        return _sceneManager->joystickAxisMoved(arg);
    }

    // InputInterface needs to know when this is completed
    return false;
}

bool Kernel::joystickPovMoved(const Input::JoystickEvent& arg) {
    if (!_platformContext.gui().joystickPovMoved(arg)) {
        return _sceneManager->joystickPovMoved(arg);
    }

    // InputInterface needs to know when this is completed
    return false;
}

bool Kernel::joystickButtonPressed(const Input::JoystickEvent& arg) {
    if (!_platformContext.gui().joystickButtonPressed(arg)) {
        return _sceneManager->joystickButtonPressed(arg);
    }

    // InputInterface needs to know when this is completed
    return false;
}

bool Kernel::joystickButtonReleased(const Input::JoystickEvent& arg) {
    if (!_platformContext.gui().joystickButtonReleased(arg)) {
        return _sceneManager->joystickButtonReleased(arg);
    }

    // InputInterface needs to know when this is completed
    return false;
}

bool Kernel::joystickBallMoved(const Input::JoystickEvent& arg) {
    if (!_platformContext.gui().joystickBallMoved(arg)) {
        return _sceneManager->joystickBallMoved(arg);
    }

    // InputInterface needs to know when this is completed
    return false;
}

bool Kernel::joystickAddRemove(const Input::JoystickEvent& arg) {
    if (!_platformContext.gui().joystickAddRemove(arg)) {
        return _sceneManager->joystickAddRemove(arg);
    }

    // InputInterface needs to know when this is completed
    return false;
}

bool Kernel::joystickRemap(const Input::JoystickEvent &arg) {
    if (!_platformContext.gui().joystickRemap(arg)) {
        return _sceneManager->joystickRemap(arg);
    }

    // InputInterface needs to know when this is completed
    return false;
}

bool Kernel::onUTF8(const Input::UTF8Event& arg) {
    if_constexpr(Config::Build::ENABLE_EDITOR) {
        Editor& editor = _platformContext.editor();
        if (!_sceneManager->wantsKeyboard() && editor.onUTF8(arg)) {
            return true;
        }
    }
    
    if (!_platformContext.gui().onUTF8(arg)) {
        return _sceneManager->onUTF8(arg);
    }

    return false;
}
};

