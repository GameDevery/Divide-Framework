/*
   Copyright (c) 2015 DIVIDE-Studio
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

/*The system is similar to the one used in Torque3D (RenderPassMgr /
  RenderBinManager)
  as it was used as an inspiration.
  All credit goes to GarageGames for the idea:
  - http://garagegames.com/
  - https://github.com/GarageGames/Torque3D
  */

#ifndef _RENDER_BIN_H_
#define _RENDER_BIN_H_

#include "Core/Math/Headers/MathMatrices.h"
#include <BetterEnums/include/enum.h>

namespace Divide {

class SceneGraphNode;
class RenderingComponent;
enum class RenderStage : U32;

struct RenderBinItem {
    RenderingComponent* _renderable;
    I32 _sortKeyA;
    I32 _sortKeyB;
    U32 _stateHash;
    F32 _distanceToCameraSq;

    RenderBinItem() : _sortKeyA(-1), _sortKeyB(-1), _stateHash(0), _distanceToCameraSq(-1.0f), _renderable(nullptr) {}
    RenderBinItem(I32 sortKeyA, I32 sortKeyB, F32 distToCamSq,
                  RenderingComponent& renderable);
};

struct RenderingOrder {
    enum class List : U32 {
        NONE = 0,
        FRONT_TO_BACK = 1,
        BACK_TO_FRONT = 2,
        BY_STATE = 3,
        COUNT
    };
};

BETTER_ENUM(RenderBinType, U32, 
    RBT_TERRAIN = 0,
    RBT_OPAQUE,
    RBT_SKY,
    RBT_WATER,
    RBT_VEGETATION_GRASS,
    RBT_TRANSLUCENT,
    RBT_PARTICLES,
    RBT_DECALS,
    RBT_IMPOSTOR,
    COUNT)

enum class RenderBitProperty : U32 {
    TRANSLUCENT = toBit(1)
};

class SceneRenderState;
/// This class contains a list of "RenderBinItem"'s and stores them sorted
/// depending on designation
class RenderBin {
    typedef vectorImpl<RenderBinItem> RenderBinStack;

   public:

    friend class RenderQueue;

    RenderBin(RenderBinType rbType,
              RenderingOrder::List renderOrder);

    virtual ~RenderBin() {}

    virtual void sort(RenderStage renderStage);
    virtual void preRender(RenderStage renderStage);
    virtual void render(const SceneRenderState& renderState,
                        RenderStage renderStage);
    virtual void postRender(const SceneRenderState& renderState,
                            RenderStage renderStage);
    virtual void refresh();

    virtual void addNodeToBin(SceneGraphNode& sgn,
                              const vec3<F32>& eyePos);

    inline const RenderBinItem& getItem(U16 index) const {
        assert(index < _renderBinStack.size());
        return _renderBinStack[index];
    }

    inline U16 getBinSize() const { return (U16)_renderBinStack.size(); }

    inline RenderBinType getType() const { return _rbType; }

    inline void binIndex(U32 index) { _binIndex = index; }

   private:
    bool isTranslucent() const;

   private:
    // mutable SharedLock _renderBinGetMutex;
    U32 _binIndex;
    RenderBinType _rbType;
    RenderBinStack _renderBinStack;
    RenderingOrder::List _renderOrder;
};

};  // namespace Divide
#endif