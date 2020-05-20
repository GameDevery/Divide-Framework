#include "stdafx.h"

#include "Headers/Editor.h"
#include "Headers/Sample.h"
#include "Headers/UndoManager.h"

#include "Editor/Widgets/Headers/MenuBar.h"
#include "Editor/Widgets/Headers/StatusBar.h"
#include "Editor/Widgets/Headers/EditorOptionsWindow.h"

#include "Editor/Widgets/DockedWindows/Headers/OutputWindow.h"
#include "Editor/Widgets/DockedWindows/Headers/PostFXWindow.h"
#include "Editor/Widgets/DockedWindows/Headers/PropertyWindow.h"
#include "Editor/Widgets/DockedWindows/Headers/SceneViewWindow.h"
#include "Editor/Widgets/DockedWindows/Headers/ContentExplorerWindow.h"
#include "Editor/Widgets/DockedWindows/Headers/SolutionExplorerWindow.h"

#include "Editor/Widgets/Headers/ImGuiExtensions.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/StringHelper.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Resources/Headers/ResourceCache.h"

#include "Managers/Headers/SceneManager.h"
#include "Managers/Headers/FrameListenerManager.h"

#include "Platform/File/Headers/FileManagement.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Textures/Headers/Texture.h"
#include "Platform/Video/Headers/RenderStateBlock.h"

#include "Rendering/Camera/Headers/Camera.h"

#include "Geometry/Shapes/Headers/Mesh.h"

#include "ECS/Components/Headers/TransformComponent.h"

#include <imgui_internal.h>
#include <imgui/addons/imgui_memory_editor/imgui_memory_editor.h>

namespace Divide {

namespace {
    const char* g_editorSaveFile = "Editor.xml";
    const char* g_editorSaveFileBak = "Editor.xml.bak";

    WindowManager* g_windowManager = nullptr;

    struct ImGuiViewportData
    {
        DisplayWindow*  _window = nullptr;
        bool            _windowOwned = false;
    };


    struct TextureCallbackData {
        vec4<I32> _colourData = { 1, 1, 1, 1 };
        vec2<F32> _depthRange = { 0.f, 1.f };
        GFXDevice* _gfxDevice = nullptr;
        bool _isDepthTexture = false;
        bool _flip = false;
    };

    hashMap<I64, TextureCallbackData> g_modalTextureData;
};

void InitBasicImGUIState(ImGuiIO& io) noexcept {
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.KeyMap[ImGuiKey_Tab] = to_I32(Input::KeyCode::KC_TAB);
    io.KeyMap[ImGuiKey_LeftArrow] = to_I32(Input::KeyCode::KC_LEFT);
    io.KeyMap[ImGuiKey_RightArrow] = to_I32(Input::KeyCode::KC_RIGHT);
    io.KeyMap[ImGuiKey_UpArrow] = to_I32(Input::KeyCode::KC_UP);
    io.KeyMap[ImGuiKey_DownArrow] = to_I32(Input::KeyCode::KC_DOWN);
    io.KeyMap[ImGuiKey_PageUp] = to_I32(Input::KeyCode::KC_PGUP);
    io.KeyMap[ImGuiKey_PageDown] = to_I32(Input::KeyCode::KC_PGDOWN);
    io.KeyMap[ImGuiKey_Home] = to_I32(Input::KeyCode::KC_HOME);
    io.KeyMap[ImGuiKey_End] = to_I32(Input::KeyCode::KC_END);
    io.KeyMap[ImGuiKey_Delete] = to_I32(Input::KeyCode::KC_DELETE);
    io.KeyMap[ImGuiKey_Backspace] = to_I32(Input::KeyCode::KC_BACK);
    io.KeyMap[ImGuiKey_Enter] = to_I32(Input::KeyCode::KC_RETURN);
    io.KeyMap[ImGuiKey_KeyPadEnter] = to_I32(Input::KeyCode::KC_NUMPADENTER);
    io.KeyMap[ImGuiKey_Escape] = to_I32(Input::KeyCode::KC_ESCAPE);
    io.KeyMap[ImGuiKey_Space] = to_I32(Input::KeyCode::KC_SPACE);
    io.KeyMap[ImGuiKey_A] = to_I32(Input::KeyCode::KC_A);
    io.KeyMap[ImGuiKey_C] = to_I32(Input::KeyCode::KC_C);
    io.KeyMap[ImGuiKey_V] = to_I32(Input::KeyCode::KC_V);
    io.KeyMap[ImGuiKey_X] = to_I32(Input::KeyCode::KC_X);
    io.KeyMap[ImGuiKey_Y] = to_I32(Input::KeyCode::KC_Y);
    io.KeyMap[ImGuiKey_Z] = to_I32(Input::KeyCode::KC_Z);

    io.SetClipboardTextFn = SetClipboardText;
    io.GetClipboardTextFn = GetClipboardText;
    io.ClipboardUserData = nullptr;
}

std::array<Input::MouseButton, 5> Editor::g_oisButtons = {
        Input::MouseButton::MB_Left,
        Input::MouseButton::MB_Right,
        Input::MouseButton::MB_Middle,
        Input::MouseButton::MB_Button3,
        Input::MouseButton::MB_Button4,
};

Editor::Editor(PlatformContext& context, ImGuiStyleEnum theme)
    : PlatformContextComponent(context),
      FrameListener("Editor", context.kernel().frameListenerMgr(), 9999),
      _currentTheme(theme),
      _editorUpdateTimer(Time::ADD_TIMER("Editor Update Timer")),
      _editorRenderTimer(Time::ADD_TIMER("Editor Render Timer"))
{
    _menuBar = eastl::make_unique<MenuBar>(context, true);
    _statusBar = eastl::make_unique<StatusBar>(context);
    _optionsWindow = eastl::make_unique<EditorOptionsWindow>(context);

    _undoManager = eastl::make_unique<UndoManager>(25);
    g_windowManager = &context.app().windowManager();
    _memoryEditorData = std::make_pair(nullptr, 0);
}

Editor::~Editor()
{
    close();
    for (DockedWindow* window : _dockedWindows) {
        MemoryManager::SAFE_DELETE(window);
    }

    g_windowManager = nullptr;
}

void Editor::idle() {
    OPTICK_EVENT();
}

bool Editor::init(const vec2<U16>& renderResolution) {
    ACKNOWLEDGE_UNUSED(renderResolution);

    if (isInit()) {
        // double init
        return false;
    }
    
    createDirectories((Paths::g_saveLocation + Paths::Editor::g_saveLocation).c_str());
    _mainWindow = &_context.app().windowManager().getWindow(0u);

    IMGUI_CHECKVERSION();
    assert(_imguiContexts[to_base(ImGuiContextType::Editor)] == nullptr);

    _imguiContexts[to_base(ImGuiContextType::Editor)] = ImGui::CreateContext();
    ImGuiIO& io = _imguiContexts[to_base(ImGuiContextType::Editor)]->IO;

    U8* pPixels = nullptr;
    I32 iWidth = 0;
    I32 iHeight = 0;
    io.Fonts->AddFontDefault();
    io.Fonts->GetTexDataAsRGBA32(&pPixels, &iWidth, &iHeight);

    SamplerDescriptor sampler = {};
    sampler.minFilter(TextureFilter::LINEAR);
    sampler.magFilter(TextureFilter::LINEAR);

    TextureDescriptor texDescriptor(TextureType::TEXTURE_2D, GFXImageFormat::RGBA, GFXDataFormat::UNSIGNED_BYTE);
    texDescriptor.samplerDescriptor(sampler);

    ResourceDescriptor resDescriptor("IMGUI_font_texture");
    resDescriptor.threaded(false);
    resDescriptor.flag(true);
    resDescriptor.propertyDescriptor(texDescriptor);

    ResourceCache* parentCache = _context.kernel().resourceCache();
    _fontTexture = CreateResource<Texture>(parentCache, resDescriptor);
    assert(_fontTexture);
    _fontTexture->loadData({(Byte*)pPixels, iWidth * iHeight * 4 }, vec2<U16>(iWidth, iHeight));

    ShaderModuleDescriptor vertModule = {};
    vertModule._moduleType = ShaderType::VERTEX;
    vertModule._sourceFile = "IMGUI.glsl";

    ShaderModuleDescriptor fragModule = {};
    fragModule._moduleType = ShaderType::FRAGMENT;
    fragModule._sourceFile = "IMGUI.glsl";

    ShaderProgramDescriptor shaderDescriptor = {};
    shaderDescriptor._modules.push_back(vertModule);
    shaderDescriptor._modules.push_back(fragModule);

    ResourceDescriptor shaderResDescriptor("IMGUI");
    shaderResDescriptor.propertyDescriptor(shaderDescriptor);
    _imguiProgram = CreateResource<ShaderProgram>(parentCache, shaderResDescriptor);

    // Store our identifier
    io.Fonts->TexID = (void *)(intptr_t)_fontTexture->data()._textureHandle;

    ImGui::ResetStyle(_currentTheme);

    if (_context.config().gui.imgui.multiViewportEnabled) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
        io.ConfigViewportsNoDecoration = !_context.config().gui.imgui.windowDecorationsEnabled;
        io.ConfigViewportsNoTaskBarIcon = true;
        io.ConfigViewportsNoAutoMerge = _context.config().gui.imgui.dontMergeFloatingWindows;
        io.ConfigDockingTransparentPayload = true;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking

        io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
    }

    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;        // We can honor io.WantSetMousePos requests (optional, rarely used)
    io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;  // We can create multi-viewports on the Platform side (optional)
    io.BackendPlatformName = "Divide Framework";
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    InitBasicImGUIState(io);

    io.DisplaySize.x = to_F32(_mainWindow->getDimensions().width);
    io.DisplaySize.y = to_F32(_mainWindow->getDimensions().height);

    const vec2<U16> display_size = _mainWindow->getDrawableSize();
    io.DisplayFramebufferScale.x = io.DisplaySize.x > 0 ? ((F32)display_size.width / io.DisplaySize.x)  : 0.f;
    io.DisplayFramebufferScale.y = io.DisplaySize.y > 0 ? ((F32)display_size.height / io.DisplaySize.y) : 0.f;

    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = _mainWindow;

    if (_context.config().gui.imgui.multiViewportEnabled) {
        ImGuiPlatformIO& platform_io = _imguiContexts[to_base(ImGuiContextType::Editor)]->PlatformIO;
        platform_io.Platform_CreateWindow = [](ImGuiViewport* viewport)
        {
            if (g_windowManager != nullptr) {
                const DisplayWindow& window = g_windowManager->getWindow(0u);
                WindowDescriptor winDescriptor = {};
                winDescriptor.title = "No Title Yet";
                winDescriptor.targetDisplay = to_U32(window.currentDisplayIndex());
                winDescriptor.clearColour.set(0.0f, 0.0f, 0.0f, 1.0f);
                winDescriptor.flags = to_U16(WindowDescriptor::Flags::HIDDEN) | 
                                      to_U16(WindowDescriptor::Flags::CLEAR_COLOUR) |
                                      to_U16(WindowDescriptor::Flags::CLEAR_DEPTH);
                // We don't enable SDL_WINDOW_RESIZABLE because it enforce windows decorations
                winDescriptor.flags |= (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? 0 : to_U32(WindowDescriptor::Flags::DECORATED);
                winDescriptor.flags |= (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? 0 : to_U32(WindowDescriptor::Flags::RESIZEABLE);
                winDescriptor.flags |= (viewport->Flags & ImGuiViewportFlags_TopMost) ? to_U32(WindowDescriptor::Flags::ALWAYS_ON_TOP) : 0;
                winDescriptor.flags |= to_U32(WindowDescriptor::Flags::SHARE_CONTEXT);

                winDescriptor.dimensions.set(viewport->Size.x, viewport->Size.y);
                winDescriptor.position.set(viewport->Pos.x, viewport->Pos.y);
                winDescriptor.externalClose = true;
                winDescriptor.targetAPI = window.context().gfx().renderAPI();

                ErrorCode err = ErrorCode::NO_ERR;
                DisplayWindow* newWindow = g_windowManager->createWindow(winDescriptor, err);
                if (err == ErrorCode::NO_ERR) {
                    assert(newWindow != nullptr);

                    newWindow->hidden(false);
                    newWindow->bringToFront();

                    newWindow->addEventListener(WindowEvent::CLOSE_REQUESTED, [viewport](const DisplayWindow::WindowEventArgs& args) noexcept {
                        ACKNOWLEDGE_UNUSED(args);
                        viewport->PlatformRequestClose = true;
                        return true; 
                    });

                    newWindow->addEventListener(WindowEvent::MOVED, [viewport](const DisplayWindow::WindowEventArgs& args) noexcept {
                        ACKNOWLEDGE_UNUSED(args);
                        viewport->PlatformRequestMove = true;
                        return true;
                    });

                    newWindow->addEventListener(WindowEvent::RESIZED, [viewport](const DisplayWindow::WindowEventArgs& args) noexcept {
                        ACKNOWLEDGE_UNUSED(args);
                        viewport->PlatformRequestResize = true;
                        return true;
                    });

                    viewport->PlatformHandle = (void*)newWindow;
                    viewport->PlatformUserData = IM_NEW(ImGuiViewportData){newWindow, true};
                } else {
                    DIVIDE_UNEXPECTED_CALL("Editor::Platform_CreateWindow failed!");
                    g_windowManager->destroyWindow(newWindow);
                }
            }
        };

        platform_io.Platform_DestroyWindow = [](ImGuiViewport* viewport)
        {
            if (g_windowManager != nullptr) {
                if (ImGuiViewportData* data = (ImGuiViewportData*)viewport->PlatformUserData)
                {
                    if (data->_window && data->_windowOwned) {
                        g_windowManager->destroyWindow(data->_window);
                    }
                    data->_window = nullptr;
                    IM_DELETE(data);
                }
                viewport->PlatformUserData = viewport->PlatformHandle = nullptr;
            }
        };

        platform_io.Platform_ShowWindow = [](ImGuiViewport* viewport) {
            if (ImGuiViewportData* data = (ImGuiViewportData*)viewport->PlatformUserData)
            {
                data->_window->hidden(false);
            }
        };

        platform_io.Platform_SetWindowPos = [](ImGuiViewport* viewport, ImVec2 pos) {
            if (ImGuiViewportData* data = (ImGuiViewportData*)viewport->PlatformUserData)
            {
                data->_window->setPosition((I32)pos.x, (I32)pos.y);
            }
        };

        platform_io.Platform_GetWindowPos = [](ImGuiViewport* viewport) -> ImVec2 {
            if (ImGuiViewportData* data = (ImGuiViewportData*)viewport->PlatformUserData)
            {
                const vec2<I32>& pos = data->_window->getPosition();
                return ImVec2((F32)pos.x, (F32)pos.y);
            }
            DIVIDE_UNEXPECTED_CALL("Editor::Platform_GetWindowPos failed!");
            return {};
        };

        platform_io.Platform_GetWindowSize = [](ImGuiViewport* viewport) -> ImVec2 {
            if (ImGuiViewportData* data = (ImGuiViewportData*)viewport->PlatformUserData) {
                const vec2<U16>& dim = data->_window->getDimensions();
                return ImVec2((F32)dim.width, (F32)dim.height);
            }
            DIVIDE_UNEXPECTED_CALL("Editor::Platform_GetWindowSize failed!");
            return {};
        };

        platform_io.Platform_GetWindowFocus = [](ImGuiViewport* viewport) -> bool {
            if (ImGuiViewportData* data = (ImGuiViewportData*)viewport->PlatformUserData) {
                return data->_window->hasFocus();
            }
            DIVIDE_UNEXPECTED_CALL("Editor::Platform_GetWindowFocus failed!");
            return false;
        };

        platform_io.Platform_SetWindowAlpha = [](ImGuiViewport* viewport, float alpha) {
            if (ImGuiViewportData* data = (ImGuiViewportData*)viewport->PlatformUserData) {
                data->_window->opacity(to_U8(alpha * 255));
            }
        };

        platform_io.Platform_SetWindowSize = [](ImGuiViewport* viewport, ImVec2 size) {
            if (ImGuiViewportData* data = (ImGuiViewportData*)viewport->PlatformUserData) {
                U16 w = to_U16(size.x);
                U16 h = to_U16(size.y);
                WAIT_FOR_CONDITION(data->_window->setDimensions(w, h));
            }
        };

        platform_io.Platform_SetWindowFocus = [](ImGuiViewport* viewport) {
            if (ImGuiViewportData* data = (ImGuiViewportData*)viewport->PlatformUserData) {
                data->_window->bringToFront();
            }
        };

        platform_io.Platform_SetWindowTitle = [](ImGuiViewport* viewport, const char* title) {
            if (ImGuiViewportData* data = (ImGuiViewportData*)viewport->PlatformUserData) {
                data->_window->title(title);
            }
        };

        platform_io.Platform_RenderWindow = [](ImGuiViewport* viewport, void* platformContext) {
            if (PlatformContext* context = (PlatformContext*)platformContext) {
                context->gfx().beginFrame(*(DisplayWindow*)viewport->PlatformHandle, false);
            }
        };

        platform_io.Renderer_RenderWindow = [](ImGuiViewport* viewport, void* platformContext) {
            if (PlatformContext* context = (PlatformContext*)platformContext) {
                Editor* editor = &context->editor();

                ImGui::SetCurrentContext(editor->_imguiContexts[to_base(ImGuiContextType::Editor)]);
                GFX::ScopedCommandBuffer sBuffer = GFX::allocateScopedCommandBuffer();
                GFX::CommandBuffer& buffer = sBuffer();
                const ImGuiIO& io = ImGui::GetIO();
                ImDrawData* pDrawData = viewport->DrawData;
                const I32 fb_width = to_I32(pDrawData->DisplaySize.x * io.DisplayFramebufferScale.x);
                const I32 fb_height = to_I32(pDrawData->DisplaySize.y * io.DisplayFramebufferScale.y);
                editor->renderDrawList(viewport->DrawData, Rect<I32>(0, 0, fb_width, fb_height), ((DisplayWindow*)viewport->PlatformHandle)->getGUID(), buffer);
                context->gfx().flushCommandBuffer(buffer);
            }
        };

        platform_io.Platform_SwapBuffers = [](ImGuiViewport* viewport, void* platformContext) {
            if (g_windowManager != nullptr) {
                PlatformContext* context = (PlatformContext*)platformContext;
                context->gfx().endFrame(*(DisplayWindow*)viewport->PlatformHandle, false);
            }
        };

        const vectorEASTL<WindowManager::MonitorData>& monitors = g_windowManager->monitorData();
        const I32 monitorCount = to_I32(monitors.size());

        platform_io.Monitors.resize(monitorCount);

        for (I32 i = 0; i < monitorCount; ++i) {
            const WindowManager::MonitorData& monitor = monitors[i];
            ImGuiPlatformMonitor& imguiMonitor = platform_io.Monitors[i];

            // Warning: the validity of monitor DPI information on Windows depends on the application DPI awareness settings, which generally needs to be set in the manifest or at runtime.
            imguiMonitor.MainPos = ImVec2((F32)monitor.viewport.x, (F32)monitor.viewport.y);
            imguiMonitor.WorkPos = ImVec2((F32)monitor.drawableArea.x, (F32)monitor.drawableArea.y);

            imguiMonitor.MainSize = ImVec2((F32)monitor.viewport.z, (F32)monitor.viewport.w);
            imguiMonitor.WorkSize = ImVec2((F32)monitor.drawableArea.z, (F32)monitor.drawableArea.w);
            imguiMonitor.DpiScale = monitor.dpi / 96.0f;
        }

        ImGuiViewportData* data = IM_NEW(ImGuiViewportData)();
        data->_window = _mainWindow;
        data->_windowOwned = false;
        main_viewport->PlatformUserData = data;
    }

    ImGuiContext*& gizmoContext = _imguiContexts[to_base(ImGuiContextType::Gizmo)];
    gizmoContext = ImGui::CreateContext(io.Fonts);
    InitBasicImGUIState(gizmoContext->IO);
    gizmoContext->Viewports[0]->PlatformHandle = _mainWindow;
    _gizmo = eastl::make_unique<Gizmo>(*this, gizmoContext);

    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    DockedWindow::Descriptor descriptor = {};
    descriptor.position = ImVec2(0, 0);
    descriptor.size = ImVec2(300, 550);
    descriptor.minSize = ImVec2(200, 200);
    descriptor.name = "Solution Explorer";
    _dockedWindows[to_base(WindowType::SolutionExplorer)] = MemoryManager_NEW SolutionExplorerWindow(*this, _context, descriptor);

    descriptor.position = ImVec2(0, 0);
    descriptor.minSize = ImVec2(200, 200);
    descriptor.name = "PostFX Settings";
    _dockedWindows[to_base(WindowType::PostFX)] = MemoryManager_NEW PostFXWindow(*this, _context, descriptor);

    descriptor.position = ImVec2(to_F32(renderResolution.width) - 300, 0);
    descriptor.name = "Property Explorer";
    _dockedWindows[to_base(WindowType::Properties)] = MemoryManager_NEW PropertyWindow(*this, _context, descriptor);

    descriptor.position = ImVec2(0, 550.0f);
    descriptor.size = ImVec2(to_F32(renderResolution.width * 0.5f), to_F32(renderResolution.height) - 550 - 3);
    descriptor.name = "Content Explorer";
    descriptor.flags |= ImGuiWindowFlags_NoTitleBar;
    _dockedWindows[to_base(WindowType::ContentExplorer)] = MemoryManager_NEW ContentExplorerWindow(*this, descriptor);

    descriptor.position = ImVec2(to_F32(renderResolution.width * 0.5f), 550);
    descriptor.size = ImVec2(to_F32(renderResolution.width * 0.5f), to_F32(renderResolution.height) - 550 - 3);
    descriptor.name = "Application Output";
    _dockedWindows[to_base(WindowType::Output)] = MemoryManager_NEW OutputWindow(*this, descriptor);

    descriptor.position = ImVec2(150, 150);
    descriptor.size = ImVec2(640, 480);
    descriptor.name = "Scene View";
    descriptor.minSize = ImVec2(100, 100);
    descriptor.flags = 0;
    _dockedWindows[to_base(WindowType::SceneView)] = MemoryManager_NEW SceneViewWindow(*this, descriptor);

    loadFromXML();

    return true;
}

void Editor::close() {
    _fontTexture.reset();
    _imguiProgram.reset();
    _gizmo.reset();

    for (ImGuiContext* context : _imguiContexts) {
        if (context == nullptr) {
            continue;
        }

        ImGui::SetCurrentContext(context);
        ImGui::DestroyPlatformWindows();
        ImGui::DestroyContext(context);
    }
    _imguiContexts.fill(nullptr);
}

void Editor::updateCameraSnapshot() {
    const Camera* playerCam = Attorney::SceneManagerCameraAccessor::playerCamera(*_context.kernel().sceneManager());
    if (playerCam != nullptr) {
        _cameraSnapshots[playerCam->getGUID()] = playerCam->snapshot();
    }
}

void Editor::onPreviewFocus(const bool state) {
    ImGuiIO& io = ImGui::GetIO();
    if (state) {
        io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
        _context.kernel().sceneManager()->onGainFocus();
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_NavNoCaptureKeyboard;
        _context.kernel().sceneManager()->onLostFocus();
    }
}

void Editor::toggle(const bool state) {
    if (running() == state) {
        return;
    }
    Scene& activeScene = _context.kernel().sceneManager()->getActiveScene();

    running(state);

    if (!state) {
        scenePreviewHovered(false);
        scenePreviewFocused(false);
        onPreviewFocus(false);

        if (!_autoSaveCamera) {
            Camera* playerCam = Attorney::SceneManagerCameraAccessor::playerCamera(*_context.kernel().sceneManager());
            if (playerCam != nullptr) {
                const auto it = _cameraSnapshots.find(playerCam->getGUID());
                if (it != std::end(_cameraSnapshots)) {
                    playerCam->fromSnapshot(it->second);
                }
            }
        }

        _context.config().save();
        activeScene.state().renderState().disableOption(SceneRenderState::RenderOptions::SCENE_GIZMO);
        activeScene.state().renderState().disableOption(SceneRenderState::RenderOptions::SELECTION_GIZMO);
        activeScene.state().renderState().disableOption(SceneRenderState::RenderOptions::ALL_GIZMOS);
        _context.kernel().sceneManager()->resetSelection(0);
        _stepQueue = 2;
    } else {
        _stepQueue = 0;
        activeScene.state().renderState().enableOption(SceneRenderState::RenderOptions::SCENE_GIZMO);
        activeScene.state().renderState().enableOption(SceneRenderState::RenderOptions::SELECTION_GIZMO);
        updateCameraSnapshot();
        static_cast<ContentExplorerWindow*>(_dockedWindows[to_base(WindowType::ContentExplorer)])->init();
        Selections selections = activeScene.getCurrentSelection();
        if (selections._selectionCount == 0) {
            //SceneGraphNode& root = activeScene.sceneGraph().getRoot();
            //_context.kernel().sceneManager()->setSelected(0, { &root });
        }
    }

    _gizmo->enable(state && simulationPauseRequested());
}

void Editor::update(const U64 deltaTimeUS) {
    OPTICK_EVENT();
    static bool allGizmosEnabled = false;

    Time::ScopedTimer timer(_editorUpdateTimer);

    for (ImGuiContext* context : _imguiContexts) {
        ImGui::SetCurrentContext(context);

        ImGuiIO& io = context->IO;
        io.DeltaTime = Time::MicrosecondsToSeconds<F32>(deltaTimeUS);

        ToggleCursor(!io.MouseDrawCursor);
        if (io.MouseDrawCursor || ImGui::GetMouseCursor() == ImGuiMouseCursor_None) {
            WindowManager::SetCursorStyle(CursorStyle::NONE);
        }else if (io.MousePos.x != -1.f && io.MousePos.y != -1.f) {
            switch (ImGui::GetCurrentContext()->MouseCursor)
            {
                case ImGuiMouseCursor_Arrow:
                    WindowManager::SetCursorStyle(CursorStyle::ARROW);
                    break;
                case ImGuiMouseCursor_TextInput:         // When hovering over InputText, etc.
                    WindowManager::SetCursorStyle(CursorStyle::TEXT_INPUT);
                    break;
                case ImGuiMouseCursor_ResizeAll:         // Unused
                    WindowManager::SetCursorStyle(CursorStyle::RESIZE_ALL);
                    break;
                case ImGuiMouseCursor_ResizeNS:          // Unused
                    WindowManager::SetCursorStyle(CursorStyle::RESIZE_NS);
                    break;
                case ImGuiMouseCursor_ResizeEW:          // When hovering over a column
                    WindowManager::SetCursorStyle(CursorStyle::RESIZE_EW);
                    break;
                case ImGuiMouseCursor_ResizeNESW:        // Unused
                    WindowManager::SetCursorStyle(CursorStyle::RESIZE_NESW);
                    break;
                case ImGuiMouseCursor_ResizeNWSE:        // When hovering over the bottom-right corner of a window
                    WindowManager::SetCursorStyle(CursorStyle::RESIZE_NWSE);
                    break;
                case ImGuiMouseCursor_Hand:
                    WindowManager::SetCursorStyle(CursorStyle::HAND);
                    break;
            }
        }
    }
    
    Attorney::GizmoEditor::update(*_gizmo, deltaTimeUS);
    if (running()) {
        _statusBar->update(deltaTimeUS);
        _optionsWindow->update(deltaTimeUS);

        static_cast<ContentExplorerWindow*>(_dockedWindows[to_base(WindowType::ContentExplorer)])->update(deltaTimeUS);


        if (_isScenePaused != simulationPauseRequested()) {
            _isScenePaused = simulationPauseRequested();

            _gizmo->enable(_isScenePaused);
            SceneManager* sMgr = _context.kernel().sceneManager();
            sMgr->resetSelection(0);
            Scene& activeScene = sMgr->getActiveScene();
            
            if (_isScenePaused) {
                activeScene.state().renderState().enableOption(SceneRenderState::RenderOptions::SCENE_GIZMO);
                activeScene.state().renderState().enableOption(SceneRenderState::RenderOptions::SELECTION_GIZMO);
                if (allGizmosEnabled) {
                    activeScene.state().renderState().enableOption(SceneRenderState::RenderOptions::ALL_GIZMOS);
                }
            } else {
                allGizmosEnabled = activeScene.state().renderState().isEnabledOption(SceneRenderState::RenderOptions::ALL_GIZMOS);
                activeScene.state().renderState().disableOption(SceneRenderState::RenderOptions::SCENE_GIZMO);
                activeScene.state().renderState().disableOption(SceneRenderState::RenderOptions::SELECTION_GIZMO);
                activeScene.state().renderState().disableOption(SceneRenderState::RenderOptions::ALL_GIZMOS);
            }
        }
    }
}

bool Editor::frameStarted(const FrameEvent& evt) {
    ACKNOWLEDGE_UNUSED(evt);
    return true;
}

bool Editor::framePreRenderStarted(const FrameEvent& evt) {
    ACKNOWLEDGE_UNUSED(evt);
    return true;
}

bool Editor::framePreRenderEnded(const FrameEvent& evt) {
    ACKNOWLEDGE_UNUSED(evt);
    return true;
}

bool Editor::frameRenderingQueued(const FrameEvent& evt) {
    ACKNOWLEDGE_UNUSED(evt);
    return true;
}

bool Editor::render(const U64 deltaTime) {
    ACKNOWLEDGE_UNUSED(deltaTime);

    const F32 statusBarHeight = _statusBar->height();
    
    static ImGuiDockNodeFlags opt_flags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode;
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size + ImVec2(0.0f, -statusBarHeight));
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    windowFlags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    const bool optionsVisible = _showOptionsWindow;

    if (opt_flags & ImGuiDockNodeFlags_PassthruCentralNode) {
        ImGui::SetNextWindowBgAlpha(0.0f);
    }
    if (windowFlags & ImGuiDockNodeFlags_PassthruCentralNode) {
        windowFlags |= ImGuiWindowFlags_NoBackground;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Editor", NULL, windowFlags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    const ImGuiID dockspaceId = ImGui::GetID("EditorDockspace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), opt_flags);

    if (scenePreviewFocused() || optionsVisible) {
        PushReadOnly();
    }

    _menuBar->draw();
    _statusBar->draw();

    for (DockedWindow* window : _dockedWindows) {
        window->draw();
    }

    if (_showMemoryEditor && !optionsVisible) {
        if (_memoryEditorData.first != nullptr && _memoryEditorData.second > 0) {
            static MemoryEditor memEditor;
            memEditor.DrawWindow("Memory Editor", _memoryEditorData.first, _memoryEditorData.second);
        }
    }

    if (_showSampleWindow && !optionsVisible) {
        ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
        ImGui::ShowDemoWindow(&_showSampleWindow);
    }

    if (scenePreviewFocused() || optionsVisible) {
        PopReadOnly();
    }

    _optionsWindow->draw(_showOptionsWindow);

    ImGui::End();

    return true;
}

void Editor::drawScreenOverlay(const Camera& camera, const Rect<I32>& targetViewport, GFX::CommandBuffer& bufferInOut) {
    Attorney::GizmoEditor::render(*_gizmo, camera, targetViewport, bufferInOut);
}

bool Editor::frameSceneRenderEnded(const FrameEvent& evt) {
    ACKNOWLEDGE_UNUSED(evt);
    return true;
}

bool Editor::framePostRenderStarted(const FrameEvent& evt) {
    for (DockedWindow* window : _dockedWindows) {
        window->backgroundUpdate();
    }

    if (!running()) {
        return true;
    }

    Time::ScopedTimer timer(_editorRenderTimer);
    ImGui::SetCurrentContext(_imguiContexts[to_base(ImGuiContextType::Editor)]);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        io.MouseHoveredViewport = 0;
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        for (I32 n = 0; n < platform_io.Viewports.Size; n++) {
            ImGuiViewport* viewport = platform_io.Viewports[n];
            const DisplayWindow* window = (DisplayWindow*)viewport->PlatformHandle;
            if (window != nullptr && window->isHovered() && !(viewport->Flags & ImGuiViewportFlags_NoInputs)) {
                ImGui::GetIO().MouseHoveredViewport = viewport->ID;
            }
        }
    }

    ImGui::NewFrame();

    if (render(evt._timeSinceLastFrameUS)) {
        ImGui::Render();

        GFX::ScopedCommandBuffer sBuffer = GFX::allocateScopedCommandBuffer();
        GFX::CommandBuffer& buffer = sBuffer();
        ImDrawData* pDrawData = ImGui::GetDrawData();
        const I32 fb_width = to_I32(pDrawData->DisplaySize.x * ImGui::GetIO().DisplayFramebufferScale.x);
        const I32 fb_height = to_I32(pDrawData->DisplaySize.y * ImGui::GetIO().DisplayFramebufferScale.y);
        renderDrawList(pDrawData, Rect<I32>(0, 0, fb_width, fb_height), _mainWindow->getGUID(), buffer);
        _context.gfx().flushCommandBuffer(buffer);


        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault(&context(), &context());
        }

        return true;
    }

    return false;
}

bool Editor::framePostRenderEnded(const FrameEvent& evt) {
    ACKNOWLEDGE_UNUSED(evt);
    return true;
}

bool Editor::frameEnded(const FrameEvent& evt) {
    ACKNOWLEDGE_UNUSED(evt);
    if (running() && _stepQueue > 0) {
        --_stepQueue;
    }

    return true;
}

const Rect<I32>& Editor::scenePreviewRect(bool globalCoords) const {
    const SceneViewWindow* sceneView = static_cast<SceneViewWindow*>(_dockedWindows[to_base(WindowType::SceneView)]);
    return sceneView->sceneRect(globalCoords);
}

// Needs to be rendered immediately. *IM*GUI. IMGUI::NewFrame invalidates this data
void Editor::renderDrawList(ImDrawData* pDrawData, const Rect<I32>& targetViewport, I64 windowGUID, GFX::CommandBuffer& bufferInOut)
{
    if (windowGUID == -1) {
        windowGUID = _mainWindow->getGUID();
    }

    const ImGuiIO& io = ImGui::GetIO();

    if (targetViewport.z <= 0 || targetViewport.w <= 0) {
        return;
    }

    pDrawData->ScaleClipRects(io.DisplayFramebufferScale);

    if (pDrawData->CmdListsCount == 0) {
        return;
    }

    RenderStateBlock state = {};
    state.setCullMode(CullMode::NONE);
    state.depthTestEnabled(false);
    state.setScissorTest(true);

    PipelineDescriptor pipelineDesc = {};
    pipelineDesc._stateHash = state.getHash();
    pipelineDesc._shaderProgramHandle = _imguiProgram->getGUID();

    GFX::EnqueueCommand(bufferInOut, GFX::BeginDebugScopeCommand{ "Render IMGUI" });

    GFX::SetBlendCommand blendCmd = {};
    blendCmd._blendProperties = BlendingProperties{
        BlendProperty::SRC_ALPHA,
        BlendProperty::INV_SRC_ALPHA,
        BlendOperation::ADD
    };
    blendCmd._blendProperties._enabled = true;
    GFX::EnqueueCommand(bufferInOut, blendCmd);

    GFX::BindPipelineCommand pipelineCmd = {};
    pipelineCmd._pipeline = _context.gfx().newPipeline(pipelineDesc);
    GFX::EnqueueCommand(bufferInOut, pipelineCmd);

    PushConstants pushConstants = {};
    pushConstants.set(_ID("toggleChannel"), GFX::PushConstantType::IVEC4, vec4<I32>(1, 1, 1, 1));
    pushConstants.set(_ID("depthTexture"), GFX::PushConstantType::INT, 0);
    pushConstants.set(_ID("depthRange"), GFX::PushConstantType::VEC2, vec2<F32>(0.0f, 1.0f));

    GFX::SendPushConstantsCommand pushConstantsCommand = {};
    pushConstantsCommand._constants = pushConstants;
    GFX::EnqueueCommand(bufferInOut, pushConstantsCommand);

    GFX::SetViewportCommand viewportCmd = {};
    viewportCmd._viewport = targetViewport;
    GFX::EnqueueCommand(bufferInOut, viewportCmd);

    const F32 L = pDrawData->DisplayPos.x;
    const F32 R = pDrawData->DisplayPos.x + pDrawData->DisplaySize.x;
    const F32 T = pDrawData->DisplayPos.y;
    const F32 B = pDrawData->DisplayPos.y + pDrawData->DisplaySize.y;
    const F32 ortho_projection[4][4] =
    {
        { 2.0f / (R - L),    0.0f,               0.0f,   0.0f },
        { 0.0f,              2.0f / (T - B),     0.0f,   0.0f },
        { 0.0f,              0.0f,              -1.0f,   0.0f },
        { (R + L) / (L - R), (T + B) / (B - T),  0.0f,   1.0f },
    };

    GFX::SetCameraCommand cameraCmd = {};
    cameraCmd._cameraSnapshot = Camera::utilityCamera(Camera::UtilityCamera::_2D_FLIP_Y)->snapshot();
    memcpy(cameraCmd._cameraSnapshot._projectionMatrix.m, ortho_projection, sizeof(F32) * 16);
    GFX::EnqueueCommand(bufferInOut, cameraCmd);

    GFX::DrawIMGUICommand drawIMGUI = {};
    drawIMGUI._data = pDrawData;
    drawIMGUI._windowGUID = windowGUID;
    GFX::EnqueueCommand(bufferInOut, drawIMGUI);

    blendCmd._blendProperties._enabled = false;
    GFX::EnqueueCommand(bufferInOut, blendCmd);

    GFX::EnqueueCommand(bufferInOut, GFX::EndDebugScopeCommand{});
}

void Editor::selectionChangeCallback(PlayerIndex idx, const vectorEASTL<SceneGraphNode*>& nodes) {
    if (idx != 0) {
        return;
    }

    Attorney::GizmoEditor::updateSelection(*_gizmo, nodes);
}

bool Editor::Undo() {
    if (_undoManager->Undo()) {
        showStatusMessage(Util::StringFormat("Undo: %s", _undoManager->lasActionName().c_str()), Time::SecondsToMilliseconds<F32>(2.0f));
        return true;
    }

    return false;
}

bool Editor::Redo() {
    if (_undoManager->Redo()) {
        showStatusMessage(Util::StringFormat("Redo: %s", _undoManager->lasActionName().c_str()), Time::SecondsToMilliseconds<F32>(2.0f));
        return true;
    }

    return false;
}

/// Key pressed: return true if input was consumed
bool Editor::onKeyDown(const Input::KeyEvent& key) {
    if (!isInit() || !running() || (!inEditMode() && scenePreviewFocused())) {
        return false;
    }

    if (scenePreviewFocused()) {
        return _gizmo->onKey(true, key);
    } else {
        ImGuiIO& io = _imguiContexts[to_base(ImGuiContextType::Editor)]->IO;

        io.KeysDown[to_I32(key._key)] = true;
        if (key._text != nullptr) {
            io.AddInputCharactersUTF8(key._text);
        }

        if (key._key == Input::KeyCode::KC_LCONTROL || key._key == Input::KeyCode::KC_RCONTROL) {
            io.KeyCtrl = true;
        }
        if (key._key == Input::KeyCode::KC_LSHIFT || key._key == Input::KeyCode::KC_RSHIFT) {
            io.KeyShift = true;
        }
        if (key._key == Input::KeyCode::KC_LMENU || key._key == Input::KeyCode::KC_RMENU) {
            io.KeyAlt = true;
        }
        if (key._key == Input::KeyCode::KC_LWIN || key._key == Input::KeyCode::KC_RWIN) {
            io.KeySuper = true;
        }
    }

    return wantsKeyboard();
}

// Key released: return true if input was consumed
bool Editor::onKeyUp(const Input::KeyEvent& key) {
    if (!isInit() || !running() || (!inEditMode() && scenePreviewFocused())) {
        return false;
    }

    ImGuiIO& io = _imguiContexts[to_base(ImGuiContextType::Editor)]->IO;
    if (io.KeyCtrl) {
        if (key._key == Input::KeyCode::KC_Z) {
            Undo();
        } else if (key._key == Input::KeyCode::KC_Y) {
            Redo();
        }
    }

    if (scenePreviewFocused()) {
        return _gizmo->onKey(false, key);
    } else {
        io.KeysDown[to_I32(key._key)] = false;

        if (key._key == Input::KeyCode::KC_LCONTROL || key._key == Input::KeyCode::KC_RCONTROL) {
            io.KeyCtrl = false;
        }

        if (key._key == Input::KeyCode::KC_LSHIFT || key._key == Input::KeyCode::KC_RSHIFT) {
            io.KeyShift = false;
        }

        if (key._key == Input::KeyCode::KC_LMENU || key._key == Input::KeyCode::KC_RMENU) {
            io.KeyAlt = false;
        }

        if (key._key == Input::KeyCode::KC_LWIN || key._key == Input::KeyCode::KC_RWIN) {
            io.KeySuper = false;
        }
    }

    return wantsKeyboard();
}

ImGuiViewport* Editor::findViewportByPlatformHandle(ImGuiContext* context, DisplayWindow* window) {
    if (window != nullptr) {
        for (I32 i = 0; i != context->Viewports.Size; i++) {
            DisplayWindow* it = (DisplayWindow*)context->Viewports[i]->PlatformHandle;

            if (it != nullptr && it->getGUID() == window->getGUID()) {
                return context->Viewports[i];
            }
        }
    }

    return nullptr;
}

/// Mouse moved: return true if input was consumed
bool Editor::mouseMoved(const Input::MouseMoveEvent& arg) {
    if (!isInit() || !running() || WindowManager::IsRelativeMouseMode()) {
        return false;
    }

    if (!arg.wheelEvent()) {
        ImGuiViewport* viewport = nullptr;
                
        DisplayWindow* focusedWindow = g_windowManager->getFocusedWindow();
        if (focusedWindow == nullptr) {
            focusedWindow = g_windowManager->mainWindow();
        }

        const bool multiViewport = _context.config().gui.imgui.multiViewportEnabled;
        ImGuiContext* editorContext = _imguiContexts[to_base(ImGuiContextType::Editor)];
        ImGuiContext* gizmoContext = _imguiContexts[to_base(ImGuiContextType::Gizmo)];

        if (multiViewport) {
            assert(editorContext->IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable);
            viewport = findViewportByPlatformHandle(editorContext, focusedWindow);
        }

        vec2<I32> mPosGlobal(-1);
        Rect<I32> viewportSize(-1);
        WindowManager::GetMouseState(mPosGlobal, true);
        if (viewport == nullptr) {
            mPosGlobal -= focusedWindow->getPosition();
            viewportSize = {mPosGlobal.x, mPosGlobal.y, focusedWindow->getDrawableSize().x, focusedWindow->getDrawableSize().y };
        } else {
            viewportSize = {viewport->Pos.x, viewport->Pos.y, viewport->Size.x, viewport->Size.y };
        }

        bool anyDown = false;
        for (U8 i = 0; i < to_U8(ImGuiContextType::COUNT); ++i) {
            ImGuiIO& io = _imguiContexts[i]->IO;

            bool mouseSet = false;
            if (io.WantSetMousePos) {
                if (multiViewport && io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                    mouseSet = WindowManager::SetGlobalCursorPosition((I32)io.MousePos.x, (I32)io.MousePos.y);
                } else {
                    mouseSet = g_windowManager->setCursorPosition((I32)io.MousePos.x, (I32)io.MousePos.y);
                }
            } else if (i == to_base(ImGuiContextType::Editor)) {
                io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
                mouseSet = true;
            }

            io.MousePos = ImVec2((F32)mPosGlobal.x, (F32)mPosGlobal.y);
            for (size_t j = 0; j < 5; ++j) {
                if (io.MouseDown[j]) {
                    anyDown = true;
                    break;
                }
            }
        }
        WindowManager::SetCaptureMouse(anyDown);
        SceneViewWindow* sceneView = static_cast<SceneViewWindow*>(_dockedWindows[to_base(WindowType::SceneView)]);
        const ImVec2 editorMousePos = _imguiContexts[to_base(ImGuiContextType::Editor)]->IO.MousePos;
        scenePreviewHovered(sceneView->isHovered() && sceneView->sceneRect(true).contains(editorMousePos.x, editorMousePos.y));

        vec2<I32> gizmoMousePos(gizmoContext->IO.MousePos.x, gizmoContext->IO.MousePos.y);
        const Rect<I32>& sceneRect = scenePreviewRect(true);
        gizmoMousePos = COORD_REMAP(gizmoMousePos, sceneRect, Rect<I32>(0, 0, viewportSize.z, viewportSize.w));
        if (!scenePreviewHovered()) {
            //gizmoMousePos = COORD_REMAP(gizmoMousePos, viewportSize, sceneRect);
        }
        showStatusMessage(Util::StringFormat("Before: [%d - %d] After: [%d - %d]", to_I32(gizmoContext->IO.MousePos.x), to_I32(gizmoContext->IO.MousePos.y), gizmoMousePos.x, gizmoMousePos.y), 400);
        gizmoContext->IO.MousePos = ImVec2(to_F32(gizmoMousePos.x), to_F32(gizmoMousePos.y));
        if (_gizmo->hovered()) {
            return true;
        }
    } else {
        for (ImGuiContext* ctx : _imguiContexts) {
            if (arg.WheelH() > 0) {
                ctx->IO.MouseWheelH += 1;
            }
            if (arg.WheelH() < 0) {
                ctx->IO.MouseWheelH -= 1;
            }
            if (arg.WheelV() > 0) {
                ctx->IO.MouseWheel += 1;
            }
            if (arg.WheelV() < 0) {
                ctx->IO.MouseWheel -= 1;
            }
        }
    }

    return wantsMouse();
}

/// Mouse button pressed: return true if input was consumed
bool Editor::mouseButtonPressed(const Input::MouseButtonEvent& arg) {
    if (!isInit() || !running() || WindowManager::IsRelativeMouseMode()) {
        return false;
    }

    for (ImGuiContext* ctx : _imguiContexts) {
        for (U8 i = 0; i < 5; ++i) {
            if (arg.button() == g_oisButtons[i]) {
                ctx->IO.MouseDown[i] = true;
                break;
            }
        }
    }

    if (scenePreviewFocused()) {
        _gizmo->onMouseButton(true);
    }
    
    return wantsMouse();
}

/// Mouse button released: return true if input was consumed
bool Editor::mouseButtonReleased(const Input::MouseButtonEvent& arg) {
    if (!isInit() || !running() || WindowManager::IsRelativeMouseMode()) {
        return false;
    }

    if (scenePreviewFocused() != scenePreviewHovered()) {
        scenePreviewFocused(scenePreviewHovered());
        onPreviewFocus(scenePreviewHovered());

        ImGuiContext* editorContext = _imguiContexts[to_base(ImGuiContextType::Editor)];
        ImGui::SetCurrentContext(editorContext);
        if (!_autoFocusEditor) {
            return !scenePreviewHovered();
        }
    }

    for (ImGuiContext* ctx : _imguiContexts) {
        for (U8 i = 0; i < 5; ++i) {
            if (arg.button() == g_oisButtons[i]) {
                ctx->IO.MouseDown[i] = false;
                break;
            }
        }
    }

    _gizmo->onMouseButton(false);

    return wantsMouse();
}

bool Editor::joystickButtonPressed(const Input::JoystickEvent &arg) {
    ACKNOWLEDGE_UNUSED(arg);

    if (!isInit() || !running()) {
        return false;
    }

    return wantsGamepad();
}

bool Editor::joystickButtonReleased(const Input::JoystickEvent &arg) {
    ACKNOWLEDGE_UNUSED(arg);

    if (!isInit() || !running()) {
        return false;
    }

    return wantsGamepad();
}

bool Editor::joystickAxisMoved(const Input::JoystickEvent &arg) {
    ACKNOWLEDGE_UNUSED(arg);

    if (!isInit() || !running()) {
        return false;
    }

    return wantsGamepad();
}

bool Editor::joystickPovMoved(const Input::JoystickEvent &arg) {
    ACKNOWLEDGE_UNUSED(arg);

    if (!isInit() || !running()) {
        return false;
    }

    return wantsGamepad();
}

bool Editor::joystickBallMoved(const Input::JoystickEvent &arg) {
    ACKNOWLEDGE_UNUSED(arg);

    if (!isInit() || !running()) {
        return false;
    }

    return wantsGamepad();
}

bool Editor::joystickAddRemove(const Input::JoystickEvent &arg) {
    ACKNOWLEDGE_UNUSED(arg);

    if (!isInit() || !running()) {
        return false;
    }

    return wantsGamepad();
}

bool Editor::joystickRemap(const Input::JoystickEvent &arg) {
    ACKNOWLEDGE_UNUSED(arg);

    if (!isInit() || !running()) {
        return false;
    }

    return wantsGamepad();
}

bool Editor::wantsMouse() const {
    if (!isInit() || !running()) {
        return false;
    }

    if (scenePreviewFocused()) {
        if (!simulationPauseRequested()) {
            return false;
        }

        return _gizmo->needsMouse();
    }

    for (ImGuiContext* ctx : _imguiContexts) {
        if (ctx->IO.WantCaptureMouse) {
            return true;
        }
    }

    return false;
}

bool Editor::wantsKeyboard() const {
    if (!isInit() || !running() || scenePreviewFocused()) {
        return false;
    }
    if (scenePreviewFocused() && !simulationPauseRequested()) {
        return false;
    }
    for (ImGuiContext* ctx : _imguiContexts) {
        if (ctx->IO.WantCaptureKeyboard) {
            return true;
        }
    }

    return false;
}

bool Editor::wantsGamepad() const {
    if (!isInit() || !running()) {
        return false;
    }

    return !scenePreviewFocused();
}

bool Editor::usingGizmo() const {
    if (!isInit() || !running()) {
        return false;
    }

    return _gizmo->needsMouse();
}

bool Editor::onUTF8(const Input::UTF8Event& arg) {
    if (!isInit() || !running() || scenePreviewFocused()) {
        return false;
    }

    bool wantsCapture = false;
    for (U8 i = 0; i < to_U8(ImGuiContextType::COUNT); ++i) {
        ImGuiIO& io = _imguiContexts[i]->IO;
        io.AddInputCharactersUTF8(arg._text);
        wantsCapture = io.WantCaptureKeyboard || wantsCapture;
    }

    return wantsCapture;
}

void Editor::onSizeChange(const SizeChangeParams& params) {
    if (!params.isWindowResize) {
        _targetViewport.set(0, 0, params.width, params.height);
    } else if (_mainWindow != nullptr && params.winGUID == _mainWindow->getGUID()) {
        if (!isInit()) {
            return;
        }

        const vec2<U16> displaySize = _mainWindow->getDrawableSize();

        for (U8 i = 0; i < to_U8(ImGuiContextType::COUNT); ++i) {
            ImGuiIO& io = _imguiContexts[i]->IO;
            io.DisplaySize.x = (F32)params.width;
            io.DisplaySize.y = (F32)params.height;
            io.DisplayFramebufferScale = ImVec2(params.width > 0 ? ((F32)displaySize.width / params.width) : 0.f,
                                                params.height > 0 ? ((F32)displaySize.height / params.height) : 0.f);
        }
    }
}

bool Editor::saveSceneChanges(DELEGATE<void, std::string_view> msgCallback, DELEGATE<void, bool> finishCallback) {
    if (_context.kernel().sceneManager()->saveActiveScene(false, true, msgCallback, finishCallback)) {
        if (saveToXML()) {
            _context.config().save();
            return true;
        }
    }

    return false;
}

U32 Editor::saveItemCount() const noexcept {
    U32 ret = 10u; // All of the scene stuff (settings, music, etc)

    const SceneGraph& graph = _context.kernel().sceneManager()->getActiveScene().sceneGraph();
    ret += graph.getTotalNodeCount();

    return ret;
}

bool Editor::modalTextureView(const char* modalName, const Texture_ptr& tex, const vec2<F32>& dimensions, bool preserveAspect, bool useModal) {

    if (tex == nullptr) {
        return false;
    }

    ImDrawCallback toggleColours { [](const ImDrawList* parent_list, const ImDrawCmd* cmd) -> void {
        ACKNOWLEDGE_UNUSED(parent_list);

        TextureCallbackData  data = *static_cast<TextureCallbackData*>(cmd->UserCallbackData);

        GFX::ScopedCommandBuffer sBuffer = GFX::allocateScopedCommandBuffer();
        GFX::CommandBuffer& buffer = sBuffer();

        PushConstants pushConstants = {};
        pushConstants.set(_ID("toggleChannel"), GFX::PushConstantType::IVEC4, data._colourData);
        pushConstants.set(_ID("depthTexture"), GFX::PushConstantType::INT, data._isDepthTexture ? 1 : 0);
        pushConstants.set(_ID("depthRange"), GFX::PushConstantType::VEC2, data._depthRange);
        pushConstants.set(_ID("flip"), GFX::PushConstantType::INT, data._flip ? 1 : 0);

        GFX::SendPushConstantsCommand pushConstantsCommand = {};
        pushConstantsCommand._constants = pushConstants;
        GFX::EnqueueCommand(buffer, pushConstantsCommand);
        data._gfxDevice->flushCommandBuffer(buffer);
    } };

    bool opened = false,  closed = false;

    if (useModal) {
        ImGui::OpenPopup(modalName);
        opened = ImGui::BeginPopupModal(modalName, NULL, ImGuiWindowFlags_AlwaysAutoResize);
    } else {
        opened = ImGui::Begin(modalName, NULL, ImGuiWindowFlags_AlwaysAutoResize);
    }

    if (opened) {
        assert(tex != nullptr);
        assert(modalName != nullptr);

        static TextureCallbackData defaultData = {};
        defaultData._gfxDevice = &_context.gfx();
        defaultData._isDepthTexture = false;
        defaultData._flip = false;

        static std::array<bool, 4> state = { true, true, true, true };

        TextureCallbackData& data = g_modalTextureData[tex->getGUID()];
        data._gfxDevice = &_context.gfx();
        data._flip = false;
        data._isDepthTexture = tex->descriptor().baseFormat() == GFXImageFormat::DEPTH_COMPONENT;

        const U8 numChannels = tex->descriptor().numChannels();

        assert(numChannels > 0);

        if (data._isDepthTexture) {
            data._flip = true; //ToDo: Investigate why - Ionut
            ImGui::Text("Depth: ");  ImGui::SameLine(); ImGui::ToggleButton("Depth", &state[0]);
            ImGui::SameLine();
            ImGui::Text("Range: "); ImGui::SameLine();
            ImGui::DragFloatRange2("", &data._depthRange[0], &data._depthRange[1], 0.005f, 0.f, 1.f);
        } else {
            ImGui::Text("R: ");  ImGui::SameLine(); ImGui::ToggleButton("R", &state[0]);

            if (numChannels > 1) {
                ImGui::SameLine();
                ImGui::Text("G: ");  ImGui::SameLine(); ImGui::ToggleButton("G", &state[1]);

                if (numChannels > 2) {
                    ImGui::SameLine();
                    ImGui::Text("B: ");  ImGui::SameLine(); ImGui::ToggleButton("B", &state[2]);
                }

                if (numChannels > 3) {
                    ImGui::SameLine();
                    ImGui::Text("A: ");  ImGui::SameLine(); ImGui::ToggleButton("A", &state[3]);
                }
            }
        }

        const bool nonDefaultColours = data._isDepthTexture || !state[0] || !state[1] || !state[2] || !state[3];
        data._colourData.set(state[0] ? 1 : 0, state[1] ? 1 : 0, state[2] ? 1 : 0, state[3] ? 1 : 0);

        if (nonDefaultColours) {
            ImGui::GetWindowDrawList()->AddCallback(toggleColours, &data);
        }

        F32 aspect = 1.0f;
        if (preserveAspect) {
            const U16 w = tex->width();
            const U16 h = tex->height();
            aspect = w / to_F32(h);
        }

        static F32 zoom = 1.0f;
        static ImVec2 zoomCenter(0.5f, 0.5f);
        ImGui::ImageZoomAndPan((void*)(intptr_t)tex->data()._textureHandle, ImVec2(dimensions.width, dimensions.height / aspect), aspect, zoom, zoomCenter, 2, 3);

        if (nonDefaultColours) {
            ImGui::GetWindowDrawList()->AddCallback(toggleColours, &defaultData);
        }

        ImGui::Text("Mouse: Wheel = scroll | CTRL + Wheel = zoom | Hold Wheel Button = pan");

        if (ImGui::Button("Close")) {
            zoom = 1.0f;
            zoomCenter = ImVec2(0.5f, 0.5f);
            if (useModal) {
                ImGui::CloseCurrentPopup();
            }
            g_modalTextureData.erase(tex->getGUID());
            closed = true;
        }

        if (useModal) {
            ImGui::EndPopup();
        } else {
            ImGui::End();
        }
    } else if (!useModal) {
        ImGui::End();
    }

    return closed;
}

bool Editor::modalModelSpawn(const char* modalName, const Mesh_ptr& mesh) {
    if (mesh == nullptr) {
        return false;
    }

    static vec3<F32> scale(1.0f);
    static char inputBuf[256] = {};

    bool closed = false;

    ImGui::OpenPopup(modalName);
    if (ImGui::BeginPopupModal(modalName, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        assert(mesh != nullptr);
        if (Util::IsEmptyOrNull(inputBuf)) {
            strcpy_s(&inputBuf[0], std::min(to_size(254), mesh->resourceName().length()) + 1, mesh->resourceName().c_str());
        }
        ImGui::Text("Spawn [ %s ]?", mesh->resourceName().c_str());
        ImGui::Separator();


        if (ImGui::InputFloat3("Scale", scale._v)) {
        }

        if (ImGui::InputText("Name", inputBuf, IM_ARRAYSIZE(inputBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        }

        ImGui::Separator();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            closed = true;
            scale.set(1.0f);
            inputBuf[0] = '\0';
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Yes", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            closed = true;
            spawnGeometry(mesh, scale, inputBuf);
            scale.set(1.0f);
            inputBuf[0] = '\0';
        }
        ImGui::EndPopup();
    }

    return closed;
}

void Editor::showStatusMessage(const stringImpl& message, F32 durationMS) {
    _statusBar->showMessage(message, durationMS);
}

bool Editor::spawnGeometry(const Mesh_ptr& mesh, const vec3<F32>& scale, const stringImpl& name) {
    constexpr U32 normalMask = to_base(ComponentType::TRANSFORM) |
                               to_base(ComponentType::BOUNDS) |
                               to_base(ComponentType::NETWORKING) |
                               to_base(ComponentType::RENDERING);

    SceneGraphNodeDescriptor nodeDescriptor = {};
    nodeDescriptor._name = name;
    nodeDescriptor._componentMask = normalMask;
    nodeDescriptor._node = mesh;

    Scene& activeScene = _context.kernel().sceneManager()->getActiveScene();
    const SceneGraphNode* node = activeScene.sceneGraph().getRoot().addChildNode(nodeDescriptor);
    if (node != nullptr) {
        const Camera* playerCam = Attorney::SceneManagerCameraAccessor::playerCamera(*_context.kernel().sceneManager());

        TransformComponent* tComp = node->get<TransformComponent>();
        tComp->setPosition(playerCam->getEye());
        tComp->rotate(RotationFromVToU(tComp->getFwdVector(), playerCam->getForwardDir()));
        tComp->setScale(scale);

        return true;
    }

    return false;
}

LightPool& Editor::getActiveLightPool() {
    Scene& activeScene = _context.kernel().sceneManager()->getActiveScene();
    return activeScene.lightPool();
}

void Editor::teleportToNode(const SceneGraphNode& sgn) const {
    Attorney::SceneManagerCameraAccessor::moveCameraToNode(*_context.kernel().sceneManager(), sgn);
}

void Editor::queueRemoveNode(I64 nodeGUID) {
    Scene& activeScene = _context.kernel().sceneManager()->getActiveScene();
    activeScene.sceneGraph().removeNode(nodeGUID);
}

bool Editor::addComponent(SceneGraphNode* selection, ComponentType newComponentType) const {
    if (selection != nullptr && newComponentType._value != ComponentType::COUNT) {
        selection->AddComponents(to_U32(newComponentType), true);
        return BitCompare(selection->componentMask(), to_U32(newComponentType));
    }

    return false;
}

bool Editor::addComponent(const Selections& selections, ComponentType newComponentType) const {
    bool ret = false;
    if (selections._selectionCount > 0) {
        const Scene& activeScene = context().kernel().sceneManager()->getActiveScene();

        for (U8 i = 0; i < selections._selectionCount; ++i) {
            SceneGraphNode* sgn = activeScene.sceneGraph().findNode(selections._selections[i]);
            ret = addComponent(sgn, newComponentType) || ret;
        }
    }

    return ret;
}

bool Editor::removeComponent(SceneGraphNode* selection, ComponentType newComponentType) const {
    if (selection != nullptr && newComponentType._value != ComponentType::COUNT) {
        selection->RemoveComponents(to_U32(newComponentType));
        return !BitCompare(selection->componentMask(), to_U32(newComponentType));
    }

    return false;
}

bool Editor::removeComponent(const Selections& selections, ComponentType newComponentType) const {
    bool ret = false;
    if (selections._selectionCount > 0) {
        const Scene& activeScene = context().kernel().sceneManager()->getActiveScene();

        for (U8 i = 0; i < selections._selectionCount; ++i) {
            SceneGraphNode* sgn = activeScene.sceneGraph().findNode(selections._selections[i]);
            ret = removeComponent(sgn, newComponentType) || ret;
        }
    }

    return ret;
}

SceneNode_ptr Editor::createNode(SceneNodeType type, const ResourceDescriptor& descriptor) {
    return Attorney::SceneManagerEditor::createNode(*context().kernel().sceneManager(), type, descriptor);
}

bool Editor::saveToXML() const {
    boost::property_tree::ptree pt;
    const Str256& editorPath = Paths::g_xmlDataLocation + Paths::Editor::g_saveLocation;

    pt.put("showMemEditor", _showMemoryEditor);
    pt.put("showSampleWindow", _showSampleWindow);
    pt.put("autoSaveCamera", _autoSaveCamera);
    pt.put("autoFocusEditor", _autoFocusEditor);
    pt.put("themeIndex", to_I32(_currentTheme));
    pt.put("textEditor", _externalTextEditorPath);

    if (createDirectory(editorPath.c_str())) {
        if (copyFile(editorPath.c_str(), g_editorSaveFile, editorPath.c_str(), g_editorSaveFileBak, true)) {
            XML::writeXML(editorPath + g_editorSaveFile, pt);
            return true;
        }
    }

    return false;
}

bool Editor::loadFromXML() {
    boost::property_tree::ptree pt;
    const Str256& editorPath = Paths::g_xmlDataLocation + Paths::Editor::g_saveLocation;
    if (!fileExists((editorPath + g_editorSaveFile).c_str())) {
        if (fileExists((editorPath + g_editorSaveFileBak).c_str())) {
            if (!copyFile(editorPath.c_str(), g_editorSaveFileBak, editorPath.c_str(), g_editorSaveFile, true)) {
                return false;
            }
        }
    }

    if (fileExists((editorPath + g_editorSaveFile).c_str())) {
        XML::readXML(editorPath + g_editorSaveFile, pt);
        _showMemoryEditor = pt.get("showMemEditor", false);
        _showSampleWindow = pt.get("showSampleWindow", false);
        _autoSaveCamera = pt.get("autoSaveCamera", false);
        _autoFocusEditor = pt.get("autoFocusEditor", true);
        _externalTextEditorPath = pt.get<stringImpl>("textEditor", "");
        _currentTheme = static_cast<ImGuiStyleEnum>(pt.get("themeIndex", to_I32(_currentTheme)));
        ImGui::ResetStyle(_currentTheme);

        return true;
    }

    return false;
}

void PushReadOnly() noexcept {
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
}

void PopReadOnly() noexcept {
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
}
}; //namespace Divide
