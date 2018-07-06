#include "Headers/SceneGraph.h"

#include "Scenes/Headers/SceneState.h"
#include "Core/Math/Headers/Transform.h"
#include "Hardware/Video/Headers/GFXDevice.h"
#include "Geometry/Shapes/Headers/Object3D.h"
#include "Geometry/Material/Headers/Material.h"
#include "Geometry/Shapes/Headers/SkinnedSubMesh.h"
#include "Environment/Water/Headers/Water.h"
#include "Environment/Terrain/Headers/Terrain.h"
#include "Hardware/Video/Shaders/Headers/ShaderProgram.h"

namespace Divide {

SceneGraphNode::SceneGraphNode( SceneGraph* const sg, SceneNode* const node, const stringImpl& name ) : GUIDWrapper(),
                                                  _sceneGraph(sg),
                                                  _node(node),
                                                  _lodLevel( 0 ),
                                                  _elapsedTime(0ULL),
                                                  _parent(nullptr),
                                                  _loaded(true),
                                                  _wasActive(true),
                                                  _active(true),
                                                  _inView(false),
                                                  _selected(false),
                                                  _isSelectable(false),
                                                  _sorted(false),
                                                  _silentDispose(false),
                                                  _boundingBoxDirty(true),
                                                  _shouldDelete(false),
                                                  _castsShadows(true),
                                                  _receiveShadows(true),
                                                  _renderWireframe(false),
                                                  _renderBoundingBox(false),
                                                  _renderSkeleton(false),
                                                  _updateTimer(GETMSTIME()),
                                                  _childQueue(0),
                                                  _bbAddExclusionList(0),
                                                  _usageContext(NODE_DYNAMIC)
{
    
    assert(_node != nullptr);

    setName( name );
    _instanceID = (node->GetRef() - 1);
    Material* const materialTemplate = _node->getMaterialTpl();
    _materialInstance = materialTemplate != nullptr ? materialTemplate->clone("_instance_" + name) : nullptr;

    _components[SGNComponent::SGN_COMP_ANIMATION]  = nullptr;
    _components[SGNComponent::SGN_COMP_NAVIGATION] = New NavigationComponent(this);
    _components[SGNComponent::SGN_COMP_PHYSICS]    = New PhysicsComponent(this);

#   ifdef _DEBUG
        // Red X-axis
        _axisLines.push_back(Line(VECTOR3_ZERO, WORLD_X_AXIS * 2, vec4<U8>(255, 0, 0, 255)));
        // Green Y-axis
        _axisLines.push_back(Line(VECTOR3_ZERO, WORLD_Y_AXIS * 2, vec4<U8>(0, 255, 0, 255))); 
        // Blue Z-axis
        _axisLines.push_back(Line(VECTOR3_ZERO, WORLD_Z_AXIS * 2, vec4<U8>(0, 0, 255, 255)));
        _axisGizmo = GFX_DEVICE.getOrCreatePrimitive(false);
        // Prepare it for line rendering
        _axisGizmo->_hasLines = true;
        _axisGizmo->_lineWidth = 5.0f;
        _axisGizmo->stateHash(GFX_DEVICE.getDefaultStateBlock(true));
        _axisGizmo->paused(true);
        // Create the object containing all of the lines
        _axisGizmo->beginBatch();
        _axisGizmo->attribute4ub("inColorData", _axisLines[0]._color);
        // Set the mode to line rendering
        _axisGizmo->begin(LINES);
        // Add every line in the list to the batch
        for (const Line& line : _axisLines) {
            _axisGizmo->attribute4ub("inColorData", line._color);
            _axisGizmo->vertex( line._startPoint );
            _axisGizmo->vertex( line._endPoint );
        }
        _axisGizmo->end();
        // Finish our object
        _axisGizmo->endBatch();
#   endif
}

///If we are destroying the current graph node
SceneGraphNode::~SceneGraphNode()
{
    unload();

    PRINT_FN(Locale::get("DELETE_SCENEGRAPH_NODE"), getName().c_str());
    //delete children nodes recursively
    for ( NodeChildren::value_type& it : _children ) {
        MemoryManager::SAFE_DELETE( it.second );
    }
    for (U8 i = 0; i < SGNComponent::ComponentType_PLACEHOLDER; ++i) {
        MemoryManager::SAFE_DELETE( _components[i] );
        _components[i] = nullptr;
    }
    _children.clear();
#ifdef _DEBUG
    _axisGizmo->_canZombify = true;
#endif
}

void SceneGraphNode::addBoundingBox(const BoundingBox& bb, const SceneNodeType& type) {
    if ( !bitCompare( _bbAddExclusionList, type ) ) {
        _boundingBox.Add( bb );
        if ( _parent ) {
            _parent->getBoundingBox().setComputed( false );
        }
    }
}

SceneGraphNode* SceneGraphNode::getRoot() const {
    return _sceneGraph->getRoot();
}

void SceneGraphNode::getBBoxes(vectorImpl<BoundingBox >& boxes ) const {
    for ( const NodeChildren::value_type& it : _children ) {
        it.second->getBBoxes( boxes );
    }

    boxes.push_back(_boundingBox);
}

///When unloading the current graph node
bool SceneGraphNode::unload(){
    if ( !_loaded ) {
        return true;
    }
    //Unload every sub node recursively
    for ( NodeChildren::value_type& it : _children ) {
        it.second->unload();
    }
    //Some debug output ...
    if ( !_silentDispose && getParent() ) {
        PRINT_FN( Locale::get( "REMOVE_SCENEGRAPH_NODE" ), _node->getName().c_str(), getName().c_str() );
    }
    if (_materialInstance){
        RemoveResource(_materialInstance);
    }
    //if not root
    if ( getParent() ) {
        RemoveResource( _node );
    }

    _loaded = false;

    for ( DELEGATE_CBK<>& cbk : _deletionCallbacks ) {
        cbk();
    }
    return true;
}

///Change current SceneGraphNode's parent
void SceneGraphNode::setParent(SceneGraphNode* const parent) {
    DIVIDE_ASSERT(parent != nullptr, "SceneGraphNode error: Can't add a new node to a null parent. Top level allowed is ROOT");
    assert(parent->getGUID() != getGUID());

    if ( _parent ) {
        if ( *_parent == *parent ) {
            return;
        }
        //Remove us from the old parent's children map
        NodeChildren::iterator it = _parent->getChildren().find( getName() );
        if ( it != _parent->getChildren().end() ) {
            _parent->getChildren().erase( it );
        }
    }
    //Set the parent pointer to the new parent
    _parent = parent;
    //Add ourselves in the new parent's children map
    //Time to add it to the children map
    hashAlg::pair<hashMapImpl<stringImpl, SceneGraphNode*>::iterator, bool > result;
    //Try and add it to the map
    result = hashAlg::insert(_parent->getChildren(), hashAlg::makePair(getName(), this));
    //If we had a collision (same name?)
    if ( !result.second ) {
        ///delete the old SceneGraphNode and add this one instead
        MemoryManager::SAFE_UPDATE( ( result.first )->second, this );
    }
    //That's it. Parent Transforms will be updated in the next render pass;
}

SceneGraphNode* SceneGraphNode::addNode( SceneNode* const node, const stringImpl& name ) {
    STUBBED( "SceneGraphNode: This add/create node system is an ugly HACK so it should probably be removed soon! -Ionut" )

    if ( node->hasSGNParent() ) {
        node->AddRef();
    }
    return createNode( node, name );
}

///Add a new SceneGraphNode to the current node's child list based on a SceneNode
SceneGraphNode* SceneGraphNode::createNode( SceneNode* const node, const stringImpl& name ) {
    //assert(node != nullptr && FindResourceImpl<Resource>(node->getName()) != nullptr);
    //Create a new SceneGraphNode with the SceneNode's info
    //We need to name the new SceneGraphNode
    //If we did not supply a custom name use the SceneNode's name
    SceneGraphNode* sceneGraphNode = New SceneGraphNode( _sceneGraph, node, name.empty() ? node->getName() : name );
    //Set the current node as the new node's parent
    sceneGraphNode->setParent(this);
    //Do all the post load operations on the SceneNode
    //Pass a reference to the newly created SceneGraphNode in case we need transforms or bounding boxes
    node->postLoad(sceneGraphNode);
    //return the newly created node
    return sceneGraphNode;
}

//Remove a child node from this Node
void SceneGraphNode::removeNode(SceneGraphNode* node) {
    //find the node in the children map
    NodeChildren::iterator it = _children.find(node->getName());
    //If we found the node we are looking for
    if ( it != _children.end() ) {
        //Remove it from the map
        _children.erase( it );
    } else {
        for ( U8 i = 0; i < _children.size(); ++i ) {
            removeNode( node );
        }
    }
    //Beware. Removing a node, does no unload it!
    //Call delete on the SceneGraphNode's pointer to do that
}

//Finding a node based on the name of the SceneGraphNode or the SceneNode it holds
//Switching is done by setting sceneNodeName to false if we pass a SceneGraphNode name
//or to true if we search by a SceneNode's name
SceneGraphNode* SceneGraphNode::findNode(const stringImpl& name, bool sceneNodeName){
    //Null return value as default
    SceneGraphNode* returnValue = nullptr;
     //Make sure a name exists
    if ( !name.empty() ) {
        //check if it is the name we are looking for
        if ( ( sceneNodeName && _node->getName().compare( name ) == 0 ) || getName().compare( name ) == 0 ) {
            // We got the node!
            return this;
        }

        //The current node isn't the one we want, so recursively check all children
        for (const NodeChildren::value_type& it : _children ) {
            returnValue = it.second->findNode( name );
            // if it is not nullptr it is the node we are looking for so just pass it through
            if ( returnValue != nullptr ) {
                return returnValue;
            }
        }
    }

    // no children's name matches or there are no more children
    // so return nullptr, indicating that the node was not found yet
    return nullptr;
}

void SceneGraphNode::Intersect(const Ray& ray, F32 start, F32 end, vectorImpl<SceneGraphNode* >& selectionHits){

    if ( isSelectable() && _boundingBox.Intersect( ray, start, end ) ) {
        selectionHits.push_back( this );
    }

    for (const NodeChildren::value_type& it : _children ) {
        it.second->Intersect( ray, start, end, selectionHits );
    }
}

};