#include "Headers/SceneManager.h"
#include "Headers/AIManager.h"

#include "SceneList.h"
#include "Rendering/Headers/Renderer.h"
#include "Geometry/Importer/Headers/DVDConverter.h"

SceneManager::SceneManager() : _activeScene(NULL){
}

SceneManager::~SceneManager(){
	PRINT_FN(Locale::get("SCENE_MANAGER_DELETE"));
	PRINT_FN(Locale::get("SCENE_MANAGER_REMOVE_SCENES"));
	for_each(SceneMap::value_type& it, _sceneMap){
		it.second->unload();
		SAFE_DELETE(it.second);
	}
	_sceneMap.clear();
	//Destroy the model loader;
	DVDConverter::getInstance().DestroyInstance();
}

bool SceneManager::load(const std::string& sceneName, const vec2<U16>& resolution, Camera* const camera){
	PRINT_FN(Locale::get("SCENE_MANAGER_LOAD_SCENE_DATA"));
	//Initialize the model importer:
	if(!DVDConverter::getInstance().init()){
		return false;
	}
	XML::loadScene(sceneName,*this);
	if(!_activeScene){
		return false;
	}
	cacheResolution(resolution);
	_activeScene->preLoad();
	_activeScene->loadXMLAssets();
	PRINT_FN(Locale::get("SCENE_MANAGER_ADD_DEFAULT_CAMERA"));
	_activeScene->renderState()->updateCamera(camera);
	return _activeScene->load(sceneName);
}

Scene* SceneManager::createScene(const std::string& name){
	Scene* scene = NULL;

	if(!name.empty()){
		scene = _sceneFactory[name]();
	}
	if(scene != NULL){
		_sceneMap.insert(std::make_pair(name, scene));
	}

	return scene;
}

void SceneManager::render(const RenderStage& stage) {
	GFXDevice& GFX = GFX_DEVICE;
	GFX.setRenderStage(stage);
	assert(_activeScene != NULL);
	if(_activeScene->renderCallback().empty()){
		SceneGraph* sg = _activeScene->getSceneGraph();
		GFX.render(DELEGATE_BIND(&SceneGraph::update,sg),_activeScene->renderState());
	}else{
		GFX.render(_activeScene->renderCallback(),_activeScene->renderState());
	}

	if(bitCompare(stage,FINAL_STAGE) || bitCompare(stage,DEFERRED_STAGE)){
		// Draw bounding boxes, skeletons, axis gizmo, etc.
		GFX.debugDraw();
		// Preview depthmaps if needed
		LightManager::getInstance().previewShadowMaps();
		// Show navmeshes
		AIManager::getInstance().debugDraw(false);
	}
}