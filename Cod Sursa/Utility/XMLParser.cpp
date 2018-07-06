#include "Headers/XMLParser.h"
#include "Headers/Guardian.h"
#include "Headers/ParamHandler.h"
#include "Managers/SceneManager.h"
#include "Managers/TerrainManager.h"
#include "Rendering/common.h"

#include "SceneList.h"

namespace XML
{
	ParamHandler &par = ParamHandler::getInstance();
	using boost::property_tree::ptree;
	ptree pt;

	void loadScripts(const string& file)
	{
		Con::getInstance().printf("XML: Loading Scripts!\n");
		read_xml(file,pt);
		par.setParam("scriptLocation",pt.get("scriptLocation","XML"));
		par.setParam("assetsLocation",pt.get("assets","Assets"));
		par.setParam("scenesLocation",pt.get("scenes","Scenes"));
		par.setParam("serverAddress",pt.get("server","127.0.0.1"));

		loadConfig(par.getParam<string>("scriptLocation") + "/" + pt.get("config","config.xml"));

		read_xml(par.getParam<string>("scriptLocation") + "/" +
			     par.getParam<string>("scenesLocation") + "/Scenes.xml",pt);

		loadScene(pt.get("MainScene","MainScene")); 
	}

	void loadConfig(const string& file)
	{
		pt.clear();
		Con::getInstance().printf("XML: Loading Configuration settings file: [ %s ]\n", file.c_str());
		read_xml(file,pt);
		par.setParam("showPhysXErrors", pt.get("debug.showPhysXErrors",true));
		par.setParam("logFile",pt.get("debug.logFile","none"));
		par.setParam("memFile",pt.get("debug.memFile","none"));
		par.setParam("groundPos", pt.get("runtime.groundPos",-2000.0f));
		par.setParam("simSpeed",pt.get("runtime.simSpeed",1));
		par.setParam("windowWidth",pt.get("runtime.windowWidth",1024));
		par.setParam("windowHeight",pt.get("runtime.windowHeight",768));
		Engine::getInstance().setWindowHeight(pt.get("runtime.windowHeight",768));
		Engine::getInstance().setWindowWidth(pt.get("runtime.windowWidth",1024));
	}

	void loadScene(const string& sceneName)
	{
		pt.clear();
		Con::getInstance().printf("XML: Loading scene [ %s ]\n", sceneName.c_str());
		read_xml(par.getParam<string>("scriptLocation") + "/" +
                 par.getParam<string>("scenesLocation") + "/" +
				 sceneName + ".xml", pt);

		Scene* scene = SceneManager::getInstance().findScene(sceneName);

		if(!scene)
		{
			Con::getInstance().errorf("XML: Trying to load unsupported scene! Defaulting to default scene\n");
			scene = new MainScene();
		}

		SceneManager::getInstance().setActiveScene(scene);

		TerrainManager* terMgr = SceneManager::getInstance().getTerrainManager();
		terMgr->getGrassVisibility() = pt.get("vegetation.grassVisibility",1000.0f);
		terMgr->getTreeVisibility()  = pt.get("vegetation.treeVisibility",1000.0f);
		terMgr->getGeneralVisibility()  = pt.get("options.visibility",1000.0f);

		terMgr->getWindDirX()  = pt.get("wind.windDirX",1.0f);
		terMgr->getWindDirZ()  = pt.get("wind.windDirZ",1.0f);
		terMgr->getWindSpeed() = pt.get("wind.windSpeed",1.0f);

		loadTerrain(par.getParam<string>("scriptLocation") + "/" +
					par.getParam<string>("scenesLocation") + "/" +
					sceneName + "/" + pt.get("terrain","terrain.xml"));

		loadGeometry(par.getParam<string>("scriptLocation") + "/" +
					 par.getParam<string>("scenesLocation") + "/" +
					 sceneName + "/" + pt.get("assets","assets.xml"));
	}



	void loadTerrain(const string &file)
	{
		pt.clear();
		Con::getInstance().printf("XML: Loading terrain: [ %s ]\n",file.c_str());
		read_xml(file,pt);
		ptree::iterator it;
		typedef pair<string,string> item;
		string assetLocation = ParamHandler::getInstance().getParam<string>("assetsLocation") + "/"; 
		for (it = pt.get_child("terrainList").begin(); it != pt.get_child("terrainList").end(); it++ )
		{
			string name = it->second.data(); //The actual terrain name
			string tag = it->first.data();   //The <name> tag for valid terrains or <xmlcomment> for comments
			//Check and skip commented terrain
			if(tag.find("<xmlcomment>") != string::npos) continue;
			//Load the rest of the terrain
			TerrainInfo ter;
			ter.variables.insert(item("terrainName",name));
			ter.variables.insert(item("heightmap",assetLocation + pt.get<string>(name + ".heightmap")));
			ter.variables.insert(item("textureMap",assetLocation + pt.get<string>(name + ".textures.map")));
			ter.variables.insert(item("redTexture",assetLocation + pt.get<string>(name + ".textures.red")));
			ter.variables.insert(item("greenTexture",assetLocation + pt.get<string>(name + ".textures.green")));
			ter.variables.insert(item("blueTexture",assetLocation + pt.get<string>(name + ".textures.blue")));
			//ter.variables.insert(item("alphaTexture",assetLocation + pt.get<string>(name + ".textures.alpha"))); NotUsed
			ter.variables.insert(item("normalMap",assetLocation + pt.get<string>(name + ".textures.normalMap")));
			ter.variables.insert(item("waterCaustics",assetLocation + pt.get<string>(name + ".textures.waterCaustics")));
			ter.position.x = pt.get<F32>(name + ".position.<xmlattr>.x");
			ter.position.y = pt.get<F32>(name + ".position.<xmlattr>.y");
			ter.position.z = pt.get<F32>(name + ".position.<xmlattr>.z");
			ter.scale.x = pt.get<F32>(name + ".scale");
			ter.scale.y = pt.get<F32>(name + ".heightFactor");
			ter.active = pt.get<bool>(name + ".active");
			ter.variables.insert(item("grassMap",assetLocation + pt.get<string>(name + ".vegetation.map")));
			ter.variables.insert(item("grassBillboard1",assetLocation + pt.get<string>(name + ".vegetation.grassBillboard1")));
			ter.variables.insert(item("grassBillboard2",assetLocation + pt.get<string>(name + ".vegetation.grassBillboard2")));
			ter.variables.insert(item("grassBillboard3",assetLocation + pt.get<string>(name + ".vegetation.grassBillboard3")));
			//ter.variables.insert(item("grassBillboard1",pt.get<string>(name + ".vegetation.grassBillboard1")));
			ter.grassDensity = pt.get<U32>(name + ".vegetation.<xmlattr>.grassDensity");
			ter.treeDensity = pt.get<U32>(name + ".vegetation.<xmlattr>.treeDensity");
			ter.grassScale = pt.get<F32>(name + ".vegetation.<xmlattr>.grassScale");
			ter.treeScale = pt.get<F32>(name + ".vegetation.<xmlattr>.treeScale");
			SceneManager::getInstance().addTerrain(ter);
			
		}
		Con::getInstance().printf("XML: Number of terrains to load: %d\n",SceneManager::getInstance().getNumberOfTerrains());
	}

	void loadGeometry(const string &file)
	{
		pt.clear();
		Con::getInstance().printf("XML: Loading Geometry: [ %s ]\n",file.c_str());
		read_xml(file,pt);
		ptree::iterator it;
		string assetLocation = ParamHandler::getInstance().getParam<string>("assetsLocation")+"/";
		if(boost::optional<ptree &> geometry = pt.get_child_optional("geometry"))
		for (it = pt.get_child("geometry").begin(); it != pt.get_child("geometry").end(); ++it )
		{
			string name = it->second.data();
			string format = it->first.data();
			if(format.find("<xmlcomment>") != string::npos) continue;
			FileData model;
			model.ItemName = name;
			model.ModelName  = assetLocation + pt.get<string>(name + ".model");
			model.position.x = pt.get<F32>(name + ".position.<xmlattr>.x");
			model.position.y = pt.get<F32>(name + ".position.<xmlattr>.y");
			model.position.z = pt.get<F32>(name + ".position.<xmlattr>.z");
			model.orientation.x = pt.get<F32>(name + ".orientation.<xmlattr>.x");
			model.orientation.y = pt.get<F32>(name + ".orientation.<xmlattr>.y");
			model.orientation.z = pt.get<F32>(name + ".orientation.<xmlattr>.z");
			model.scale.x    = pt.get<F32>(name + ".scale.<xmlattr>.x"); 
			model.scale.y    = pt.get<F32>(name + ".scale.<xmlattr>.y"); 
			model.scale.z    = pt.get<F32>(name + ".scale.<xmlattr>.z"); 
			model.type = GEOMETRY;
			model.version = pt.get<F32>(name + ".version");
			SceneManager::getInstance().addModel(model);
		}
		if(boost::optional<ptree &> vegetation = pt.get_child_optional("vegetation"))
		for (it = pt.get_child("vegetation").begin(); it != pt.get_child("vegetation").end(); ++it )
		{
			string name = it->second.data();
			string format = it->first.data();
			if(format.find("<xmlcomment>") != string::npos) continue;
			FileData model;
			model.ItemName = name;
			model.ModelName  = assetLocation + pt.get<string>(name + ".model");
			model.position.x = pt.get<F32>(name + ".position.<xmlattr>.x");
			model.position.y = pt.get<F32>(name + ".position.<xmlattr>.y");
			model.position.z = pt.get<F32>(name + ".position.<xmlattr>.z");
			model.orientation.x = pt.get<F32>(name + ".orientation.<xmlattr>.x");
			model.orientation.y = pt.get<F32>(name + ".orientation.<xmlattr>.y");
			model.orientation.z = pt.get<F32>(name + ".orientation.<xmlattr>.z");
			model.scale.x    = pt.get<F32>(name + ".scale.<xmlattr>.x"); 
			model.scale.y    = pt.get<F32>(name + ".scale.<xmlattr>.y"); 
			model.scale.z    = pt.get<F32>(name + ".scale.<xmlattr>.z"); 
			model.type = VEGETATION;
			model.version = pt.get<F32>(name + ".version");
			SceneManager::getInstance().addModel(model);
		}
		if(boost::optional<ptree &> primitives = pt.get_child_optional("primitives"))
		for (it = pt.get_child("primitives").begin(); it != pt.get_child("primitives").end(); ++it )
		{
			string name = it->second.data();
			string format = it->first.data();
			if(format.find("<xmlcomment>") != string::npos) continue;

			FileData model;
			model.ItemName = name;
			model.ModelName = pt.get<string>(name + ".model");
			model.position.x = pt.get<F32>(name + ".position.<xmlattr>.x");
			model.position.y = pt.get<F32>(name + ".position.<xmlattr>.y");
			model.position.z = pt.get<F32>(name + ".position.<xmlattr>.z");
			model.orientation.x = pt.get<F32>(name + ".orientation.<xmlattr>.x");
			model.orientation.y = pt.get<F32>(name + ".orientation.<xmlattr>.y");
			model.orientation.z = pt.get<F32>(name + ".orientation.<xmlattr>.z");
			model.scale.x    = pt.get<F32>(name + ".scale.<xmlattr>.x"); 
			model.scale.y    = pt.get<F32>(name + ".scale.<xmlattr>.y"); 
			model.scale.z    = pt.get<F32>(name + ".scale.<xmlattr>.z"); 
			/*Primitives don't use materials yet so we can define colors*/
			model.color.r    = pt.get<F32>(name + ".color.<xmlattr>.r"); 
			model.color.g    = pt.get<F32>(name + ".color.<xmlattr>.g"); 
			model.color.b    = pt.get<F32>(name + ".color.<xmlattr>.b");
			/*The data variable stores a float variable (not void*) that can represent anything you want*/
			/*For Text3D, it's the line width and for Box3D it's the edge length*/
			if(model.ModelName.compare("Text3D") == 0)
			{
				model.data = pt.get<F32>(name + ".lineWidth");
				model.data2 = pt.get<string>(name + ".text");
			}
			else if(model.ModelName.compare("Box3D") == 0)
				model.data = pt.get<F32>(name + ".size");

			else if(model.ModelName.compare("Sphere3D") == 0)
				model.data = pt.get<F32>(name + ".radius");
			else
				model.data = 0;
			
			model.type = PRIMITIVE;
			model.version = pt.get<F32>(name + ".version");
			SceneManager::getInstance().addModel(model);
		}
	}
}