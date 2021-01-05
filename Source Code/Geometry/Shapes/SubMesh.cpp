#include "stdafx.h"

#include "Headers/SubMesh.h"


#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"
#include "Headers/Mesh.h"

#include "Core/Resources/Headers/ResourceCache.h"
#include "Graphs/Headers/SceneGraphNode.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Headers/RenderPackage.h"

namespace Divide {

SubMesh::SubMesh(GFXDevice& context, ResourceCache* parentCache, const size_t descriptorHash, const Str256& name, const ObjectFlag flag)
    : Object3D(context, parentCache, descriptorHash, name, {}, {}, ObjectType::SUBMESH, to_base(flag))
{
}

void SubMesh::buildDrawCommands(SceneGraphNode* sgn,
                                const RenderStagePass& renderStagePass,
                                const Camera& crtCamera,
                                RenderPackage& pkgInOut) {

    GenericDrawCommand cmd = {};
    cmd._primitiveType = PrimitiveType::TRIANGLES,
    cmd._sourceBuffer = getGeometryVB()->handle();
    cmd._cmd.firstIndex = to_U32(getGeometryVB()->getPartitionOffset(_geometryPartitionIDs[0]));
    cmd._cmd.indexCount = to_U32(getGeometryVB()->getPartitionIndexCount(_geometryPartitionIDs[0]));
    cmd._cmd.primCount = sgn->instanceCount();
    cmd._bufferIndex = renderStagePass.baseIndex();
    
    if (sgn->context().config().debug.renderFilter.meshes) {
        pkgInOut.add(GFX::DrawCommand{ cmd });
    }

    Object3D::buildDrawCommands(sgn, renderStagePass, crtCamera, pkgInOut);
}

void SubMesh::setParentMesh(Mesh* const parentMesh) {
    _parentMesh = parentMesh;
    setGeometryVB(_parentMesh->getGeometryVB());
}


void SubMesh::sceneUpdate(const U64 deltaTimeUS,
                          SceneGraphNode* sgn,
                          SceneState& sceneState) {
    Object3D::sceneUpdate(deltaTimeUS, sgn, sceneState);

}
};