#include "SceneGraph.h"
#include "RenderQueue.h"
#include "Hardware/Video/GFXDevice.h"
#include "Managers/SceneManager.h"
#include "Utility/Headers/Event.h"

SceneGraph::SceneGraph() : _scene(NULL){
	_root = New SceneGraphNode(New SceneRoot);
	_updateRunning = false;
}

void SceneGraph::update(){
	RenderQueue::getInstance().refresh();
	boost::unique_lock< boost::mutex > lock_access_here(_rootAccessMutex);
	_root->updateTransformsAndBounds();
	_root->updateVisualInformation();
}

void SceneGraph::render(){
	GFXDevice& gfx = GFXDevice::getInstance();
	//If we have computed shadowmaps, bind them before rendering any geomtry;
	//Always bind shadowmaps to slots 7 and 8;
	if(!_scene){
		_scene = SceneManager::getInstance().getActiveScene();
	}
	FrameBufferObject *dm0 = _scene->getDepthMap(0);
	FrameBufferObject *dm1 = _scene->getDepthMap(1);

	if(!gfx.getDepthMapRendering() && !gfx.getDeferredShading()){
		if(dm0)	dm0->Bind(7);
		if(dm1)	dm1->Bind(8);
	}		
	if(!_updateRunning){
		startUpdateThread();
	}

	gfx.processRenderQueue();

	if(!gfx.getDepthMapRendering()&& !gfx.getDeferredShading()){
	 	if(dm0)	dm0->Unbind(8);
		if(dm1)	dm1->Unbind(7);
	}
}

void SceneGraph::print(){
	Console::getInstance().printfn("SceneGraph: ");
	Console::getInstance().toggleTimeStamps(false);
	boost::unique_lock< boost::mutex > lock_access_here(_rootAccessMutex);
	_root->print();
	Console::getInstance().toggleTimeStamps(true);
}

void SceneGraph::startUpdateThread(){
	//New Event(1,true,false,boost::bind(&SceneGraph::update,this));
	//_updateRunning = true;
	update();
}