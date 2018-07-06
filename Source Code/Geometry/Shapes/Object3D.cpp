#include "Headers/Object3D.h"

#include "Managers/Headers/SceneManager.h"
#include "Hardware/Video/Headers/GFXDevice.h"
#include "Geometry/Material/Headers/Material.h"

Object3D::Object3D(const ObjectType& type, U32 flag) : Object3D("", type, flag)

{
}

Object3D::Object3D(const std::string& name, const ObjectType& type, U32 flag) : SceneNode(name,TYPE_OBJECT3D),
                                                                                _update(false),
                                                                                _geometryType(type),
                                                                                _geometryFlagMask(flag),
                                                                                _geometryPartitionId(0)
{
    _renderInstance = New RenderInstance(this);
    _renderInstance->buffer(bitCompare(_geometryFlagMask, OBJECT_FLAG_NO_VB) ? nullptr : GFX_DEVICE.newVB());
}

Object3D::~Object3D()
{
    if(!bitCompare(_geometryFlagMask, OBJECT_FLAG_NO_VB))
        _renderInstance->deleteBuffer();

    SAFE_DELETE(_renderInstance);
}

VertexBuffer* const Object3D::getGeometryVB() const {
    assert(_renderInstance->buffer() != nullptr);
    return _renderInstance->buffer();
}

void Object3D::render(SceneGraphNode* const sgn, const SceneRenderState& sceneRenderState){
    if(!GFX_DEVICE.excludeFromStateChange(SceneNode::getType())){
        _renderInstance->transform(sgn->getTransform());
    }
    _renderInstance->stateHash(_drawStateHash);

    if(_renderInstance->drawCommands().empty())
        _renderInstance->addDrawCommand(GenericDrawCommand());

    GFX_DEVICE.renderInstance(_renderInstance);

}

void  Object3D::postLoad(SceneGraphNode* const sgn){
    Material* mat = getMaterial();
    if (mat && bitCompare(_geometryFlagMask, OBJECT_FLAG_SKINNED))
        mat->setHardwareSkinning(true);

    SceneNode::postLoad(sgn);
}

bool Object3D::onDraw(SceneGraphNode* const sgn, const RenderStage& currentStage){
    if (getState() != RES_LOADED) 
        return false;

    if(getObjectType() == MESH)
        return true;

    //check if we need to update vb shader
    //custom shaders ALWAYS override material shaders
    if (_customShader){
        _drawShader = _customShader;
    } else {
        if (!getMaterial())
            return false;
        RenderStage shaderFlag =  bitCompare(DEPTH_STAGE, currentStage) ? ((bitCompare(currentStage, SHADOW_STAGE) ? SHADOW_STAGE : Z_PRE_PASS_STAGE)) : FINAL_STAGE;
        _drawShader = getMaterial()->getShaderInfo(shaderFlag).getProgram();
    }

    assert(_drawShader != nullptr);
    if(getGeometryVB()) getGeometryVB()->setShaderProgram(_drawShader);

    return true;
}

void Object3D::computeNormals() {
    if (!getGeometryVB()) return;

    vec3<F32> v1 , v2, normal;
    //Code from http://devmaster.net/forums/topic/1065-calculating-normals-of-a-mesh/

    vectorImpl<vec3<F32> >* normal_buffer = new vectorImpl<vec3<F32> >[getGeometryVB()->getPosition().size()];

    for( U32 i = 0; i < getGeometryVB()->getIndexCount(); i += 3 ) {
        // get the three vertices that make the faces
        const vec3<F32>& p1 = getGeometryVB()->getPosition(getGeometryVB()->getIndex(i+0));
        const vec3<F32>& p2 = getGeometryVB()->getPosition(getGeometryVB()->getIndex(i+1));
        const vec3<F32>& p3 = getGeometryVB()->getPosition(getGeometryVB()->getIndex(i+2));

        v1.set(p2 - p1);
        v2.set(p3 - p1);
        normal.cross(v1, v2);
        normal.normalize();

        // Store the face's normal for each of the vertices that make up the face.
        normal_buffer[getGeometryVB()->getIndex(i+0)].push_back( normal );
        normal_buffer[getGeometryVB()->getIndex(i+1)].push_back( normal );
        normal_buffer[getGeometryVB()->getIndex(i+2)].push_back( normal );
    }

    getGeometryVB()->resizeNormalCount((U32)getGeometryVB()->getPosition().size());
    // Now loop through each vertex vector, and average out all the normals stored.
    vec3<F32> currentNormal;
    for(U32 i = 0; i < getGeometryVB()->getPosition().size(); ++i ){
        currentNormal.reset();
        for(U32 j = 0; j < normal_buffer[i].size(); ++j ){
            currentNormal += normal_buffer[i][j];
        }
        currentNormal /= normal_buffer[i].size();

        getGeometryVB()->modifyNormalValue(i, currentNormal);
    }
    SAFE_DELETE_ARRAY(normal_buffer);
}

void Object3D::computeTangents(){
    if (!getGeometryVB()) return;

    //Code from: http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-13-normal-mapping/#header-1
    // inputs
    const vectorImpl<vec2<F32> > & uvs      = getGeometryVB()->getTexcoord();
    //const vectorImpl<vec3<F32> > & normals  = _geometry->getNormal();

    vec3<F32> deltaPos1, deltaPos2;
    vec2<F32> deltaUV1, deltaUV2;
    vec3<F32> tangent, bitangent;

    for( U32 i = 0; i < getGeometryVB()->getIndexCount(); i += 3 ) {
        // get the three vertices that make the faces
        const vec3<F32>& v0 = getGeometryVB()->getPosition(getGeometryVB()->getIndex(i+0));
        const vec3<F32>& v1 = getGeometryVB()->getPosition(getGeometryVB()->getIndex(i+1));
        const vec3<F32>& v2 = getGeometryVB()->getPosition(getGeometryVB()->getIndex(i+2));

        // Shortcuts for UVs
        const vec2<F32> & uv0 = uvs[getGeometryVB()->getIndex(i+0)];
        const vec2<F32> & uv1 = uvs[getGeometryVB()->getIndex(i+1)];
        const vec2<F32> & uv2 = uvs[getGeometryVB()->getIndex(i+2)];

        // Edges of the triangle : position delta
        deltaPos1.set(v1-v0);
        deltaPos2.set(v2-v0);

        // UV delta
        deltaUV1.set(uv1-uv0);
        deltaUV2.set(uv2-uv0);

        F32 r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
        tangent.set((deltaPos1 * deltaUV2.y   - deltaPos2 * deltaUV1.y)*r);
        bitangent.set((deltaPos2 * deltaUV1.x   - deltaPos1 * deltaUV2.x)*r);

        // Set the same tangent for all three vertices of the triangle.
        // They will be merged later, in vbindexer.cpp
        getGeometryVB()->addTangent(tangent);
        getGeometryVB()->addTangent(tangent);
        getGeometryVB()->addTangent(tangent);
        // Same thing for binormals
        getGeometryVB()->addBiTangent(bitangent);
        getGeometryVB()->addBiTangent(bitangent);
        getGeometryVB()->addBiTangent(bitangent);
    }
}

//Create a list of triangles from the vertices + indices lists based on primitive type
bool Object3D::computeTriangleList(bool force){
    VertexBuffer* geometry = _renderInstance->buffer();
    U32 partitionOffset = geometry->getPartitionOffset(_geometryPartitionId);
    U32 partitionCount  = geometry->getPartitionCount(_geometryPartitionId);
    PrimitiveType type  = (_geometryType == MESH || _geometryType == SUBMESH ? TRIANGLES : TRIANGLE_STRIP);
    //We can't have a VB without vertex positions
    DIVIDE_ASSERT(!geometry->getPosition().empty(), "Object3D error: computeTriangleList called with no position data available!");

    if(!_geometryTriangles.empty() && force) 
        _geometryTriangles.resize(0);

    if(geometry->getIndexCount() == 0) 
        return false;
    
    U32 indiceCount = partitionCount;
    bool largeIndices = geometry->usesLargeIndices();
    if(type == TRIANGLE_STRIP){
        U32 indiceStart = 2 + partitionOffset;
        U32 indiceEnd = indiceCount + partitionOffset;
        vec3<U32> curTriangle;
        _geometryTriangles.reserve(indiceCount * 0.5);
        if(largeIndices){
            const vectorImpl<U32>& indices = geometry->getIndicesL();
            for(U32 i = indiceStart; i < indiceEnd; i++){
                curTriangle.set(indices[i - 2], indices[i - 1], indices[i]);
                //Check for correct winding
                if (i % 2 != 0) std::swap(curTriangle.y, curTriangle.z);
                _geometryTriangles.push_back(curTriangle);
            }
        }else{
            const vectorImpl<U16>& indices = geometry->getIndicesS();
            for(U32 i = indiceStart; i < indiceEnd; i++){
                curTriangle.set(indices[i - 2], indices[i - 1], indices[i]);
                //Check for correct winding
                if (i % 2 != 0) std::swap(curTriangle.y, curTriangle.z);
                _geometryTriangles.push_back(curTriangle);
            }
        }
    }else if (type == TRIANGLES){
        indiceCount /= 3;
        _geometryTriangles.reserve(indiceCount);
        if(largeIndices){
            const vectorImpl<U32>& indices = geometry->getIndicesL();
            for(U32 i = 0; i < indiceCount; i++){
                    _geometryTriangles.push_back(vec3<U32>(indices[i*3 + 0],indices[i*3 + 1],indices[i*3 + 2]));
            }
        }else{
            const vectorImpl<U16>& indices = geometry->getIndicesS();
                for(U32 i = 0; i < indiceCount; i++){
                    _geometryTriangles.push_back(vec3<U32>(indices[i*3 + 0],indices[i*3 + 1],indices[i*3 + 2]));
            }
        }
    }

    //Check for degenerate triangles
    _geometryTriangles.erase(std::partition(_geometryTriangles.begin(), _geometryTriangles.end(),
                            [](const vec3<U32>& triangle) -> bool {
                            return (triangle.x != triangle.y && triangle.x != triangle.z && triangle.y != triangle.z);
                        }), _geometryTriangles.end());

    DIVIDE_ASSERT(!_geometryTriangles.empty(), "Object3D error: computeTriangleList() failed to generate any triangles!");
    return true;
}