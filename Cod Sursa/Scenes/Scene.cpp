#include "Scene.h"
#include "Managers/ResourceManager.h"
#include "PhysX/PhysX.h"
#include "ASIO.h"
#include "Geometry/Predefined/Box3D.h"
#include "Geometry/Predefined/Quad3D.h"
#include "Geometry/Predefined/Sphere3D.h"
#include "Geometry/Predefined/Text3D.h"

Scene::~Scene()
{
	for(vector<Event*>::iterator it = _events.begin(); it != _events.end(); it++)
	{
		_events.erase(it);
		delete *it;
	}
	_events.clear();
}

void Scene::clean() //Called when application is idle
{
	if(ModelArray.empty() && GeometryArray.empty()) return;

	for(unordered_map<string,DVDFile*>::iterator iter = ModelArray.begin(); iter != ModelArray.end(); iter++)
	{
		if((iter->second)->clean())
		{
			delete (iter->second);
			iter->second = NULL;
			ModelArray.erase(iter);
			break;
		}
	}
	bool _updated = false;

	for(vector<FileData>::iterator iter = PendingDataArray.begin(); iter != PendingDataArray.end(); iter++)
	{
		if(!loadModel(*iter))
		{
			WorldPacket p(CMSG_REQUEST_GEOMETRY);
			p << (*iter).ModelName;
			ASIO::getInstance().sendPacket(p);
			while(!loadModel(*iter))
			{
				Con::getInstance().printfn("Waiting for file .. ");

			}
		}
		else
		{
			vector<FileData>::iterator iter2;
			for(iter2 = ModelDataArray.begin(); iter2 != ModelDataArray.end(); iter2++)
			{
				if((*iter2).ItemName.compare((*iter).ItemName) == 0)
				{
					ModelDataArray.erase(iter2);
					ModelDataArray.push_back(*iter);
					PendingDataArray.erase(iter);
					_updated = true;
					break;
				}
			}
			if(_updated) break;
			
		}
	}
}

void Scene::addPatch(vector<FileData>& data)
{
	for(vector<FileData>::iterator iter = data.begin(); iter != data.end(); iter++)
	{
		for(unordered_map<string,DVDFile*>::iterator iter2 = ModelArray.begin(); iter2 != ModelArray.end(); iter2++)
			if((iter2->second)->getItemName().compare((*iter).ItemName) == 0)
			{
				(iter2->second)->scheduleDeletion(); //ToDo: Fix OpenGL context switch between threads; -Ionut
			
				Con::getInstance().printfn("SCENE: new pending geometry added in queue [ %s ]",(*iter).ModelName.c_str());
				//ToDo: MUTEX!!!!!!!!!!!!!!!! -Ionut
				boost::mutex::scoped_lock l(_mutex);
				PendingDataArray.push_back(*iter);
			}

		for(unordered_map<string,Object3D*>::iterator iter2 = GeometryArray.begin(); iter2 != GeometryArray.end(); iter2++)
			if((iter2->second)->getName().compare((*iter).ModelName) == 0)
			{
				//ToDo: MUTEX!!!!!!!!!!!!!!!! -Ionut
				boost::mutex::scoped_lock l(_mutex);
				PendingDataArray.push_back(*iter);
				delete iter2->second;
				iter2->second = NULL;
				GeometryArray.erase(iter2);
				break;
			}
	}
}


void Scene::setInitialData()
{
	for(vector<FileData>::iterator it = ModelDataArray.begin(); it != ModelDataArray.end();)
	{
		//vegetation is loaded elsewhere
		if((*it).type == VEGETATION)
		{
			VegetationDataArray.push_back(*it);
			it = ModelDataArray.erase(it);
		}
		else
		{
			loadModel(*it);
			++it;
		}

	}
}	

bool Scene::loadModel(const FileData& data)
{
	Con::getInstance().printfn("Loading: [ %s ]",data.ModelName.c_str());;
	if(data.type == PRIMITIVE) 
		return loadGeometry(data);

	DVDFile *thisObj = ResourceManager::getInstance().LoadResource<DVDFile>(data.ModelName);
	if (!thisObj)
	{
		Con::getInstance().errorfn("SceneManager: Error loading model [ %s ]",  data.ModelName.c_str());
		return false;
	}
	thisObj->getItemName() = data.ItemName;	
	
	
	thisObj->getTransform()->scale(data.scale);
	thisObj->getTransform()->rotateEuler(data.orientation);
	thisObj->getTransform()->translate(data.position);

	pair<unordered_map<string,DVDFile*>::iterator,bool> _result;
	_result = ModelArray.insert(pair<string,DVDFile*>(data.ItemName,thisObj));
	if(!_result.second) (_result.first)->second = thisObj;

	return true;
}

bool Scene::loadGeometry(const FileData& data)
{
	Object3D* thisObj = ResourceManager::getInstance().LoadResource<Object3D>(data.ModelName);
	if(!thisObj)
	{
		Con::getInstance().errorfn("SCENEMANAGER: Error adding unsupported geometry to scene: [ %s ]",data.ModelName.c_str());;
		return false;
	}
	thisObj->getItemName() = data.ItemName;
	if(data.ModelName.compare("Box3D") == 0) {

			((Box3D*)thisObj)->getSize() = data.data;
			((Box3D*)thisObj)->getColor() = data.color;
			((Box3D*)thisObj)->getTransform()->scale(data.scale);
			((Box3D*)thisObj)->getTransform()->rotateEuler(data.orientation);
			((Box3D*)thisObj)->getTransform()->translate(data.position);
			((Box3D*)thisObj)->getName() = data.ItemName;

	} else if(data.ModelName.compare("Sphere3D") == 0) {
			
			((Sphere3D*)thisObj)->getSize() = data.data;
			((Sphere3D*)thisObj)->getColor() = data.color;
			((Sphere3D*)thisObj)->getTransform()->scale(data.scale);
			((Sphere3D*)thisObj)->getTransform()->rotateEuler(data.orientation);
			((Sphere3D*)thisObj)->getTransform()->translate(data.position);
			((Sphere3D*)thisObj)->getName() = data.ItemName;

	} else if(data.ModelName.compare("Quad3D") == 0)	{
			vec3 scale = data.scale;
			vec3 position = data.position;

			((Quad3D*)thisObj)->getColor() = data.color;
			((Quad3D*)thisObj)->getTransform()->scale(data.scale);
			((Quad3D*)thisObj)->getTransform()->rotateEuler(data.orientation);
			((Quad3D*)thisObj)->getTransform()->translate(data.position);
			((Quad3D*)thisObj)->getName() = data.ItemName;

			((Quad3D*)thisObj)->_tl = vec3(scale.x/2-position.x,scale.y/2+position.y, scale.z/2 + position.z);
			((Quad3D*)thisObj)->_tr = vec3(scale.x/2+position.x,scale.y/2+position.y, scale.z/2 + position.z);
			((Quad3D*)thisObj)->_bl = vec3(scale.x/2-position.x,scale.y/2-position.y, scale.z/2 + position.z);
			((Quad3D*)thisObj)->_br = vec3(scale.x/2+position.x,scale.y/2-position.y, scale.z/2 + position.z);

	} else if(data.ModelName.compare("Text3D") == 0) {

			((Text3D*)thisObj)->getWidth() = data.data;
			((Text3D*)thisObj)->getColor() = data.color;
			((Text3D*)thisObj)->getTransform()->scale(data.scale);
			((Text3D*)thisObj)->getTransform()->rotateEuler(data.orientation);
			((Text3D*)thisObj)->getTransform()->translate(data.position);
			((Text3D*)thisObj)->getName() = data.ItemName;
			((Text3D*)thisObj)->getText() = data.data2;
	}
	else
	{
		Con::getInstance().errorfn("SCENEMANAGER: Error adding unsupported geometry to scene: [ %s ]",data.ModelName.c_str());
		return false;
	}

	pair<unordered_map<string,Object3D*>::iterator,bool> _result;
	_result = GeometryArray.insert(pair<string,Object3D*>(thisObj->getName(),thisObj));
	if(!_result.second) (_result.first)->second = thisObj;
	return true;
}


bool Scene::addDefaultLight()
{
	F32 oldSize = _lights.size();
	vec2 angle = vec2(0.0f, RADIANS(45.0f));
	vec4 vector = vec4(-cosf(angle.x) * sinf(angle.y),	-cosf(angle.y),	-sinf(angle.x) * sinf(angle.y),	0.0f );
	vec4 diffuseColor(1.0f,1.0f,1.0f,1.0f);
	_defaultLight = new Light(oldSize);
	_defaultLight->setLightProperties(string("position"),vector);
	_defaultLight->setLightProperties(string("ambient"),vec4(0.3f, 0.3f, 0.3f, 1.0f));
	_defaultLight->setLightProperties(string("diffuse"),diffuseColor);
	_defaultLight->setLightProperties(string("specular"),diffuseColor);
	_defaultLight->update();

	_lights.push_back(_defaultLight);
	if(_lights.size() > oldSize) return true;	
	else return false;

}

void Scene::addGeometry(Object3D* const object)
{
	GeometryArray.insert(pair<string,Object3D*>(object->getName(),object));
}

void Scene::addModel(DVDFile* const model)
{
	ModelArray.insert(pair<string,DVDFile*>(model->getName(),model));
}

bool Scene::unload()
{
	clearObjects();
	clearEvents();
	return true;
}