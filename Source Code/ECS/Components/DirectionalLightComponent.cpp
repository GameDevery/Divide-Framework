#include "stdafx.h"

#include "Headers/DirectionalLightComponent.h"
#include "Managers/Headers/SceneManager.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Graphs/Headers/SceneGraphNode.h"

namespace Divide {

namespace {
    constexpr F32 g_defaultLightDistance = 500.0f;
};

DirectionalLightComponent::DirectionalLightComponent(SceneGraphNode& sgn, PlatformContext& context)
    : BaseComponentType<DirectionalLightComponent, ComponentType::DIRECTIONAL_LIGHT>(sgn, context), 
      Light(sgn, -1, LightType::DIRECTIONAL, sgn.parentGraph().parentScene().lightPool()),
      _csmSplitCount(3),
      _csmNearClipOffset(100.0f)
{
    setRange(g_defaultLightDistance);
    _shadowProperties._lightDetails.y = to_F32(_csmSplitCount);
    _shadowProperties._lightDetails.z = 0.0f;
    csmSplitCount(context.config().rendering.shadowMapping.defaultCSMSplitCount);
    getEditorComponent().registerField("Range and Cone", &_rangeAndCones, EditorComponentFieldType::PUSH_TYPE, false, GFX::PushConstantType::VEC3);

    getEditorComponent().registerField("Direction",
                                        [this](void* dataOut) { static_cast<vec3<F32>*>(dataOut)->set(directionCache()); },
                                        [this](const void* data) { /*NOP*/ ACKNOWLEDGE_UNUSED(data); },
                                        EditorComponentFieldType::PUSH_TYPE,
                                        true,
                                        GFX::PushConstantType::VEC3);

    BoundingBox bb;
    bb.setMin(-g_defaultLightDistance * 0.5f);
    bb.setMax(-g_defaultLightDistance * 0.5f);
    Attorney::SceneNodeLightComponent::setBounds(sgn.getNode(), bb);
}

DirectionalLightComponent::~DirectionalLightComponent()
{
}

};