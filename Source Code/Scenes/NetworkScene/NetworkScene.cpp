#include "stdafx.h"

#include "Headers/NetworkScene.h"

#include "GUI/Headers/GUIButton.h"
#include "Core/Headers/ParamHandler.h"
#include "Core/Headers/StringHelper.h"
#include "Core/Headers/XMLEntryData.h"
#include "Core/Networking/Headers/LocalClient.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Time/Headers/ApplicationTimer.h"
#include "Managers/Headers/SceneManager.h"

namespace Divide {
NetworkScene::NetworkScene(PlatformContext& context, ResourceCache& cache, SceneManager& parent, const stringImpl& name)
    : Scene(context, cache, parent, name)
{
}

NetworkScene::~NetworkScene()
{
}

void NetworkScene::processInput(PlayerIndex idx, const U64 deltaTimeUS) {
    Light* light = _lightPool->getLight(0, LightType::DIRECTIONAL);
    FColour vSunColour = Lerp(FColour(1.0f, 0.5f, 0.0f, 1.0f),
                              FColour(1.0f, 1.0f, 0.8f, 1.0f),
                              0.25f + cosf(_sunAngle.y) * 0.75f);

    light->setDiffuseColour(vSunColour);

    //_currentSky = addSky();
    Scene::processInput(idx, deltaTimeUS);
}

void NetworkScene::processGUI(const U64 deltaTimeUS) {
    D64 FpsDisplay = Time::SecondsToMilliseconds(0.3);
    D64 TimeDisplay = Time::SecondsToMilliseconds(0.01);
    D64 ServerPing = Time::SecondsToMilliseconds(1.0);
    if (_guiTimersMS[0] >= FpsDisplay) {
        _GUI->modifyText(_ID("fpsDisplay"),
                         Util::StringFormat("FPS: %5.2f", Time::ApplicationTimer::instance().getFps()));
        _guiTimersMS[0] = 0.0;
    }

    if (_guiTimersMS[1] >= TimeDisplay) {
        _GUI->modifyText(_ID("timeDisplay"),
                         Util::StringFormat("Elapsed time: %5.0f", Time::ElapsedSeconds()));
        _guiTimersMS[1] = 0.0;
    }

    if (_taskTimers[2] >= ServerPing) {
        _GUI->modifyText(
            _ID("statusText"),
            (char*)_paramHandler.getParam<stringImpl>(_ID("asioStatus")).c_str());
        _GUI->modifyText(_ID("serverMessage"),
                         (char*)_paramHandler.getParam<stringImpl>(
                             _ID("serverResponse")).c_str());
        _guiTimersMS[2] = 0.0;
    }
}

void NetworkScene::checkPatches(I64 btnGUID) {
    if (_modelDataArray.empty()) return;
    WorldPacket p(OPCodesEx::CMSG_GEOMETRY_LIST);
    p << stringImpl("NetworkScene");
    p << _modelDataArray.size();

    /*for(vector<FileData>::iterator _iter = std::begin(_modelDataArray);
    _iter != std::end(_modelDataArray); ++_iter)    {
        p << (*_iter).ItemName;
        p << (*_iter).ModelName;
        p << (*_iter).version;
    }*/
    _context.client().sendPacket(p);
}

bool NetworkScene::load(const stringImpl& name) {
    // Load scene resources
    bool loadState = SCENE_LOAD(name, true, true);

    _paramHandler.setParam(_ID("serverResponse"), "waiting");
    addLight(LightType::DIRECTIONAL, _sceneGraph->getRoot());
    _currentSky = addSky();
    playerCamera()->setEye(vec3<F32>(0, 30, -30));

    return loadState;
}

void NetworkScene::test(I64 btnGUID) {
    WorldPacket p(OPCodesEx::CMSG_PING);
    p << Time::ElapsedMilliseconds();
    _context.client().sendPacket(p);
}

void NetworkScene::connect(I64 btnGUID) {
    _GUI->modifyText(_ID("statusText"), "Connecting to server ...");
    _context.client().connect(_context.entryData().serverAddress.c_str(), 443);
}

void NetworkScene::disconnect(I64 btnGUID) {
    if (!_context.client().isConnected()) {
        _GUI->modifyText(_ID("statusText"), "Disconnecting to server ...");
    }
    _context.client().disconnect();
}

bool NetworkScene::loadResources(bool continueOnErrors) {
    _sunAngle = vec2<F32>(0.0f, Angle::to_RADIANS(45.0f));
    _sunvector =
        vec4<F32>(-cosf(_sunAngle.x) * sinf(_sunAngle.y), -cosf(_sunAngle.y),
                  -sinf(_sunAngle.x) * sinf(_sunAngle.y), 0.0f);

    _guiTimersMS.push_back(0.0f);  // Fps
    _guiTimersMS.push_back(0.0f);  // Time
    _guiTimersMS.push_back(0.0f);  // Server Ping
    return true;
}

void NetworkScene::postLoadMainThread() {
    const vec2<U16>& resolution = _context.app().windowManager().getActiveWindow().getDimensions();

    _GUI->addText("fpsDisplay",  // Unique ID
        RelativePosition2D(RelativeValue(0.0f, 60.0f), RelativeValue(0.0f, 60.0f)),  // Position
        Font::DIVIDE_DEFAULT,  // Font
                  UColour(0, 164, 255, 255),  // Colour
        Util::StringFormat("FPS: %d", 0));  // Text and arguments
    _GUI->addText("timeDisplay",
        RelativePosition2D(RelativeValue(0.0f, 60.0f), RelativeValue(0.0f, 70.0f)),
        Font::DIVIDE_DEFAULT,
                  UColour(164, 64, 64, 255),
        Util::StringFormat("Elapsed time: %5.0f", Time::ElapsedSeconds()));

    _GUI->addText("serverMessage",
                  pixelPosition(resolution.width / 4,
            resolution.height / 1),
        Font::DIVIDE_DEFAULT,
                  UColour(128, 128, 64, 255),
        Util::StringFormat("Server says: %s", "<< nothing yet >>"));
    _GUI->addText("statusText",
                  pixelPosition(resolution.width / 3,
            resolution.height / 2),
        Font::DIVIDE_DEFAULT,
                  UColour(64, 128, 64, 255),
        "");

    GUIButton* btn = _GUI->addButton(_ID("getPing"),
                                     "ping me",
                                     pixelPosition(60, to_I32(resolution.height / 1.1f)),
                                     pixelScale(100, 25));
    btn->setEventCallback(GUIButton::Event::MouseClick,
                          DELEGATE_BIND(&NetworkScene::test, this, std::placeholders::_1));

    btn = _GUI->addButton(_ID("disconnect"),
                          "disconnect",
                          pixelPosition(180, to_I32(resolution.height / 1.1f)),
                          pixelScale(100, 25));
    btn->setEventCallback(GUIButton::Event::MouseClick,
                          DELEGATE_BIND(&NetworkScene::disconnect, this, std::placeholders::_1));
   
    btn = _GUI->addButton(_ID("connect"),
                          "connect",
                          pixelPosition(300, to_I32(resolution.height / 1.1f)),
                          pixelScale(100, 25));
    btn->setEventCallback(GUIButton::Event::MouseClick,
                          DELEGATE_BIND(&NetworkScene::connect, this, std::placeholders::_1));

    btn = _GUI->addButton(_ID("patch"),
                          "patch",
                          pixelPosition(420, to_I32(resolution.height / 1.1f)),
                          pixelScale(100, 25));
    btn->setEventCallback(GUIButton::Event::MouseClick,
                          DELEGATE_BIND(&NetworkScene::checkPatches, this, std::placeholders::_1));
   
    Scene::postLoadMainThread();
}

};