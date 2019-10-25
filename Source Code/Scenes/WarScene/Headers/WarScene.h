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

#ifndef _WAR_SCENE_H
#define _WAR_SCENE_H

#include "Scenes/Headers/Scene.h"

namespace Divide {

class SkinnedSubMesh;
class NPC;

namespace AI {
class AITeam;
class AIEntity;
class WarSceneOrder;
};

class WarScene : public Scene {
   public:
    explicit WarScene(PlatformContext& context, ResourceCache& cache, SceneManager& parent, const Str64& name);
    ~WarScene();

    bool load(const Str64& name) override;
    bool unload() override;
    void postLoadMainThread(const Rect<U16>& targetRenderViewport) override;
    void processTasks(const U64 deltaTimeUS) override;
    void processGUI(const U64 deltaTimeUS) override;
    void updateSceneStateInternal(const U64 deltaTimeUS);
    U16 registerInputActions() override;

    void registerPoint(U16 teamID, const stringImpl& unitName);
    void printMessage(U8 eventId, const stringImpl& unitName);
    void debugDraw(const Camera& activeCamera, RenderStagePass stagePass, GFX::CommandBuffer& bufferInOut) override;

   private:
    void onSetActive() override;
    void startSimulation(I64 btnGUID);
    void toggleCamera(InputParams param);
    bool removeUnits();
    bool addUnits();
    bool resetUnits();
    void checkGameCompletion();
    void weaponCollision(const RigidBodyComponent& collider);
    AI::AIEntity* findAI(SceneGraphNode* node);
    bool initializeAI(bool continueOnErrors);
    bool deinitializeAI(bool continueOnErrors);

    void toggleTerrainMode();

   private:
    GUIMessageBox* _infoBox;
    vector<SceneGraphNode*> _lightNodes;
    vector<std::pair<SceneGraphNode*, bool>> _lightNodes2;
    vector<SceneGraphNode*> _lightNodes3;

   private:  // Game
    U32  _timeLimitMinutes;
    U32  _scoreLimit;
    U32  _runCount;
    U64  _elapsedGameTime;
    bool _sceneReady;
    bool _resetUnits;
    bool _terrainMode;
    U64 _lastNavMeshBuildTime;
    /// NPC's are the actual game entities
    vector<SceneGraphNode*> _armyNPCs[2];
    IMPrimitive* _targetLines;
    SceneGraphNode* _flag[2];
    SceneGraphNode* _particleEmitter;
    SceneGraphNode* _firstPersonWeapon;
    /// Teams are factions for AIEntites so they can manage friend/foe situations
    AI::AITeam* _faction[2];
};

};  // namespace Divide

#endif