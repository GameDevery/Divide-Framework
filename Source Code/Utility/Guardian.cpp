#include "Headers/Guardian.h"
#include "Managers/Headers/SceneManager.h"
#include "Headers/XMLParser.h"
#include "Rendering/PostFX/Headers/PostFX.h"
#include "Dynamics/Physics/Headers/PXDevice.h"

using namespace std;

void Guardian::LoadApplication(const string& entryPoint){
	Application& app = Application::getInstance();
	///Target FPS is 60. So all movement is capped around that value
	Framerate::getInstance().Init(60);
	Console::getInstance().printCopyrightNotice();
	PRINT_FN("Starting the application!");
	//Initialize application window and hardware devices
	XML::loadScripts(entryPoint); //ToDo: This should be moved in each scene constructor! - Ionut Cava
	app.Initialize();
	LoadSettings(); //ToDo: This should be moved up so that it is the first instruction Guardian executes! - Ionut Cava
	PRINT_FN("Initializing the rendering engine");
	SceneManager::getInstance().load(string(""));
	PRINT_FN("Initial data loaded ... ");
	PRINT_FN("Creating AI entities ...");
	SceneManager::getInstance().initializeAI(true);
	PRINT_FN("AI Entities created ...");
	PRINT_FN("Entering main rendering loop ...");
	//Target FPS is 60. So all movement is capped around that value
	GFX_DEVICE.initDevice(60);
	
	_closing = false;
}

void Guardian::TerminateApplication(){

	SceneManager::getInstance().deinitializeAI(true);
	SceneManager::getInstance().DestroyInstance();
	ResourceManager::getInstance().DestroyInstance();
	PRINT_FN("Application shutdown complete!");
}

void Guardian::LoadSettings()
{
	ParamHandler &par = ParamHandler::getInstance();
    
	string mem = par.getParam<string>("memFile");
	string log = par.getParam<string>("logFile");
	XML::loadMaterialXML(par.getParam<string>("scriptLocation")+"/defaultMaterial");
	if(mem.compare("none") != 0) myfile.open(mem.c_str());
	else myfile.open("mem.log");

}
