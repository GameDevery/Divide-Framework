/*�Copyright 2009-2011 DIVIDE-Studio�*/
/* This file is part of DIVIDE Framework.

   DIVIDE Framework is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   DIVIDE Framework is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with DIVIDE Framework.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "resource.h"
#ifndef _SCENE_MANAGER_H
#define _SCENE_MANAGER_H

#include "Manager.h"
#include "Scenes/Scene.h"
#include "Utility/Headers/Singleton.h"

DEFINE_SINGLETON_EXT1(SceneManager,Manager)

public:
	Scene* getActiveScene() {return _scene;}
	void setActiveScene(Scene* scene) {if(_scene) delete _scene; _scene = scene;}

	/*Base Scene Operations*/
	inline void render() {_scene->render();}
	inline void preRender() {_scene->preRender();}
	inline bool load(const std::string& name) {_scene->setInitialData(); return _scene->load(name);}
	inline bool unload() {return _scene->unload();}
	inline void processInput(){_scene->processInput();}
	inline void processEvents(F32 time){_scene->processEvents(time);}
	/*Base Scene Operations*/

	//inline unordered_map<std::string, Object3D* >& getGeometryArray(){return _scene->getGeometryArray();}
	inline std::vector<FileData>& getModelDataArray() {return _scene->getModelDataArray();}
	inline std::vector<FileData>& getVegetationDataArray() {return _scene->getVegetationDataArray();}
	inline std::vector<Light*>& getLights(){return _scene->getLights();}
	//U32 getNumberOfObjects(){return _scene->getNumberOfObjects();}
    inline U32 getNumberOfTerrains(){return _scene->getNumberOfTerrains();}
   
	Scene* findScene(const std::string& name);

	inline void addModel(FileData& model){_scene->addModel(model);}
	inline void addTerrain(TerrainDescriptor* ter) {_scene->addTerrain(ter);}
	void toggleBoundingBoxes();
	inline void addPatch(std::vector<FileData>& data){_scene->addPatch(data);}
	
	void findSelection(U32 x, U32 y);
	void deleteSelection();

	inline void clean(){_scene->clean();}

private:

	SceneManager();
	Scene* _scene;
	std::map<std::string, Scene*> _scenes;
	std::map<std::string, Scene*>::iterator _sceneIter;
    Object3D* _currentSelection;

END_SINGLETON

#endif

