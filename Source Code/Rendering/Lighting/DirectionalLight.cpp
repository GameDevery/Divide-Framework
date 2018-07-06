#include "Headers/DirectionalLight.h"
#include "Managers/Headers/SceneManager.h"
#include "Hardware/Video/Headers/GFXDevice.h"
#include "ShadowMapping/Headers/ShadowMap.h"
#include "Core/Resources/Headers/ResourceCache.h"

namespace Divide {

DirectionalLight::DirectionalLight(U8 slot) : Light(slot,-1,LIGHT_TYPE_DIRECTIONAL),
                                              _csmSplitCount(3),
                                              _csmSplitLogFactor(0.95f),
                                              _csmStabilize(true),
                                              _csmNearClipOffset(100.0f)
{
    vec2<F32> angle(0.0f, RADIANS(45.0f));
    setDirection(vec3<F32>(-cosf(angle.x) * sinf(angle.y),-cosf(angle.y),-sinf(angle.x) * sinf(angle.y)));
}

DirectionalLight::~DirectionalLight()
{
}

};