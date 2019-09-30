#include "stdafx.h"

#include "Headers/DefaultScene.h"

#include "GUI/Headers/GUIButton.h"

#include "Core/Headers/PlatformContext.h"
#include "Managers/Headers/SceneManager.h"
#include "Rendering/PostFX/Headers/PostFX.h"

#include "Dynamics/Entities/Units/Headers/Player.h"

#include "ECS/Components/Headers/TransformComponent.h"
#include "ECS/Components/Headers/RenderingComponent.h"
#include "ECS/Components/Headers/DirectionalLightComponent.h"

#include <CEGUI/CEGUI.h>

namespace Divide {
DefaultScene::DefaultScene(PlatformContext& context, ResourceCache& cache, SceneManager& parent, const stringImpl& name)
    : Scene(context, cache, parent, name)
{
}

bool DefaultScene::load(const stringImpl& name) {
    bool loadState = SCENE_LOAD(name);
    // Add a light
    vec2<F32> sunAngle(0.0f, Angle::to_RADIANS(45.0f));
    vec3<F32> sunvector(-cosf(sunAngle.x) * sinf(sunAngle.y),
                        -cosf(sunAngle.y),
                        -sinf(sunAngle.x) * sinf(sunAngle.y));
    
    _sun->get<TransformComponent>()->setRotationEuler(sunvector);
    _currentSky->getNode<Sky>().enableSun(true, _sun->get<DirectionalLightComponent>()->getDiffuseColour(), sunvector);

    state().saveLoadDisabled(true);

    _taskTimers.push_back(0.0);

    return loadState;
}

void DefaultScene::processGUI(const U64 deltaTimeUS) {
    Scene::processGUI(deltaTimeUS);
}

void DefaultScene::postLoadMainThread(const Rect<U16>& targetRenderViewport) {
    // Replace buttons with nice, animated elements? images?
    const vector<stringImpl>& scenes = _parent.sceneNameList();

    const vec2<U16>& resolution = _context.gfx().renderingResolution();

    const I32 spacingX = 10;
    const I32 spacingY = 10;
    const I32 numColumns = 3;
    const I32 numRows = to_I32(std::ceil(to_F32(scenes.size()) / numColumns));
    const I32 btnWidth = 100;
    const I32 btnHeight = 100;
    const I32 windowCenterX = to_I32(resolution.width * 0.5f);
    const I32 windowCenterY = to_I32(resolution.height * 0.5f);
    const F32 btnStartXOffset = NORMALIZE(to_F32(windowCenterX) - numColumns * 0.5f * btnWidth, 0.0f, to_F32(resolution.width));
    const F32 btnStartYOffset = NORMALIZE(to_F32(windowCenterY) - numRows * 0.5f * btnHeight, 0.0f, to_F32(resolution.height));
    const I32 quitButtonWidth = 100;
    const I32 quitButtonHeight = 100;
    const I32 playerButtonHeight = 25;

    RelativePosition2D buttonPosition(RelativeValue(btnStartXOffset, 0.0f),
                                      RelativeValue(btnStartYOffset, 0.0f));
    // In pixels
    RelativeScale2D buttonSize(RelativeValue(0.0f, to_F32(btnWidth)),
                               RelativeValue(0.0f, to_F32(btnHeight)));

    size_t i = 0, j = 0;
    for (const stringImpl& scene : scenes) {
        if (scene != resourceName()) {
            size_t localOffsetX = btnWidth * (i % numColumns) + spacingX * (i % numColumns);
            size_t localOffsetY = btnHeight * (j % numRows) + spacingY * (j % numRows);

            buttonPosition.d_x.d_offset = to_F32(localOffsetX);
            buttonPosition.d_y.d_offset = to_F32(localOffsetY);
            GUIButton* btn = _GUI->addButton(_ID(("StartScene" + scene).c_str()),
                                             scene,
                                             buttonPosition,
                                             buttonSize);

            btn->setEventCallback(GUIButton::Event::MouseClick,
                                  [this](I64 btnGUID) {
                                      loadScene(btnGUID);

                                  });

            _buttonToSceneMap[btn->getGUID()] = scene;
            i++;
            if (i > 0 && i % numColumns == 0) {
                j++;
            }
        }
    }

    RelativePosition2D quitPosition(RelativeValue(0.0f, resolution.width - quitButtonWidth * 1.5f),
                                    RelativeValue(0.0f, resolution.height - quitButtonHeight * 1.5f));
    RelativeScale2D quitScale(RelativeValue(0.0f, to_F32(quitButtonWidth)),
                              RelativeValue(0.0f, to_F32(quitButtonHeight)));

    GUIButton* btn = _GUI->addButton(_ID("Quit"),
                                     "Quit",
                                     quitPosition,
                                     quitScale);

    btn->setEventCallback(GUIButton::Event::MouseClick,
                          [this](I64 btnGUID) {
                              _context.app().RequestShutdown();
                          });

    RelativePosition2D playerChangePosition(RelativeValue(0.01f, 0.0f),
                                            RelativeValue(0.0f, resolution.height - playerButtonHeight * 1.5f));

    RelativeScale2D playerChangeScale = pixelScale(quitButtonWidth, playerButtonHeight);

    btn = _GUI->addButton(_ID("AddPlayer"),
                    "Add Player",
                    playerChangePosition,
                    playerChangeScale);

    btn->setEventCallback(GUIButton::Event::MouseClick,
                          [this](I64 btnGUID) {
                              addPlayerInternal(true);
                          });

    playerChangePosition.d_y.d_offset -= playerButtonHeight * 1.5f;

    btn = _GUI->addButton(_ID("RemovePlayer"),
                          "Remove Player",
                          playerChangePosition,
                          playerChangeScale);

    btn->setEventCallback(GUIButton::Event::MouseClick,
                          [this](I64 btnGUID) {
                              if (_scenePlayers.size() > 1) {
                                  removePlayerInternal(_scenePlayers.back()->index());
                              }
                          });

    RelativePosition2D textPosition = pixelPosition(windowCenterX - 100, windowCenterY - ((numRows / 2) + 1)* btnHeight);
    _GUI->addText("globalMessage",
                  textPosition,
                  Font::DIVIDE_DEFAULT,
                  UColour4(128, 64, 64, 255),
                  "");

    textPosition.d_y.d_offset -= 20;

    Scene::postLoadMainThread(targetRenderViewport);
}

void DefaultScene::processInput(PlayerIndex idx, const U64 deltaTimeUS) {
    if (!_sceneToLoad.empty()) {
        const vec2<U16>& drawSize = _context.activeWindow().getDrawableSize();

        _GUI->modifyText(_ID("globalMessage"),
                         Util::StringFormat("Please wait while scene [ %s ] is loading", _sceneToLoad.c_str()),
                         false);
        _parent.switchScene(_sceneToLoad, false, Rect<U16>(0, 0, drawSize.width, drawSize.height));
        _sceneToLoad.clear();
    }

    Angle::DEGREES<F32>& angle = _camAngle[getSceneIndexForPlayer(idx)];
    if (idx % 3 == 1) {
        getPlayerForIndex(idx)->getCamera().rotatePitch(angle);
    } else if (idx % 3 == 2) {
        getPlayerForIndex(idx)->getCamera().rotateRoll(angle);
    } else {
        getPlayerForIndex(idx)->getCamera().rotateYaw(angle);
    }
    angle = 0.0f;

    Scene::processInput(idx, deltaTimeUS);
}

void DefaultScene::processTasks(const U64 deltaTimeUS) {
    constexpr D64 SpinTimer = Time::Milliseconds(16.0);

    if (_taskTimers[0] >= SpinTimer) {
        for (hashMap<U8, Angle::DEGREES<F32>>::value_type& it : _camAngle) {
            it.second = 0.25f * ((it.first * 2.0f) + 1.0f) * (it.first % 2 == 0 ? -1 : 1);
        }

        _taskTimers[0] = 0.0;
    }

    Scene::processTasks(deltaTimeUS);
}

void DefaultScene::loadScene(I64 btnGUID) {
    _sceneToLoad = _buttonToSceneMap[btnGUID];
    Console::d_printf("Loading scene [ %s ]", _sceneToLoad.c_str());

    GUIButton* selection = _GUI->getGUIElement<GUIButton>(btnGUID);
    selection->setText(_sceneToLoad + "\nLoading ...");
    for (hashMap<I64, stringImpl>::value_type it : _buttonToSceneMap) {
        GUIButton* btn = _GUI->getGUIElement<GUIButton>(it.first);
        btn->setActive(false);
        if (it.first != btnGUID) {
            btn->setVisible(false);
        }
    }
}

void DefaultScene::onSetActive() {
    for (hashMap<I64, stringImpl>::value_type it : _buttonToSceneMap) {
        GUIButton* btn = _GUI->getGUIElement<GUIButton>(it.first);
        
        btn->setText(it.second);
        btn->setActive(true);
        btn->setVisible(true);
    }

    Scene::onSetActive();
}
};
