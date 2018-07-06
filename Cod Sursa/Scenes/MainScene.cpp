#include "Hardware/Video/OpenGL/glResources.h" //ToDo: Remove this from here -Ionut

#include "MainScene.h"

#include "Utility/Headers/ParamHandler.h"
#include "Managers/ResourceManager.h"
#include "Managers/TerrainManager.h"
#include "Rendering/Framerate.h"
#include "Rendering/Camera.h"
#include "Rendering/common.h"
#include "PhysX/PhysX.h"
#include "Terrain/Sky.h"
#include "GUI/GUI.h"
#include "Rendering/Frustum.h"

bool MainScene::updateLights()
{
	_sun_cosy = cosf(_sunAngle.y);
	vec4 vSunColor = _white.lerp(vec4(1.0f, 0.5f, 0.0f, 1.0f), vec4(1.0f, 1.0f, 0.8f, 1.0f),
								0.25f + _sun_cosy * 0.75f);

	_lights[0]->setLightProperties(string("position"),_sunVector);
	_lights[0]->setLightProperties(string("ambient"),_white);
	_lights[0]->setLightProperties(string("diffuse"),vSunColor);
	_lights[0]->setLightProperties(string("specular"),vSunColor);
	//_lights[0]->update();
	return true;
}

void MainScene::preRender()
{
	ResourceManager& res = ResourceManager::getInstance();
	Sky &sky = Sky::getInstance();
	Camera& cam = Camera::getInstance();
	
	ParamHandler &par = ParamHandler::getInstance();
/*
	//SHADOW MAPPING
	GLdouble tabOrtho[2] = {20.0, 100.0};
	F32 zNear = 0.01f;//par.getParam<F32>("zNear");
	F32 zFar = 5000.0f;//par.getParam<F32>("zFar");
	vec3 eye_pos = Camera::getInstance().getEye();
	vec3 sun_pos = eye_pos - vec3(_sunVector);


	vec3 eye = Camera::getInstance().getEye();


	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	gluLookAt(	eye.x - 500*_sunVector.x,		eye.y - 500*_sunVector.y,		eye.z - 500*_sunVector.z,
				eye.x,							eye.y,							eye.z,
				0.0,							1.0,							0.0	);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	for(U32 i=0; i<2; i++)
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-tabOrtho[i], tabOrtho[i], -tabOrtho[i], tabOrtho[i], zNear, zFar);
		glMatrixMode(GL_MODELVIEW);
		Frustum::getInstance().Extract(sun_pos);
		
		_depthMap[i]->Begin();
			_GFX.clearBuffers(0x00004000| 0x00000100);
			renderActors();
			vec4 vGroundAmbient = _white.lerp(_white*0.2f, _black, _sun_cosy);
			_terMgr->drawTerrains(true,true,true,vGroundAmbient);
		_depthMap[i]->End();
	}

		glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, zNear, zFar);
	Frustum::getInstance().Extract(sun_pos);

	// On remet les matrices comme avant
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	Frustum::getInstance().Extract(eye_pos);
	
    _terMgr->setDepthMap(0,_depthMap[0]);
	_terMgr->setDepthMap(1,_depthMap[1]);

	//SHADOW MAPPING
*/
	_skyFBO->Begin();

	_GFX.clearBuffers(0x00004000/*GL_COLOR_BUFFER_BIT*/ | 0x00000100/*GL_DEPTH_BUFFER_BIT*/);

	updateLights();

	sky.setParams(Camera::getInstance().getEye(),vec3(_sunVector),true,true,true);
	vec4 vGroundAmbient = _white.lerp(_white*0.2f, _black, _sun_cosy);
	
	sky.draw();
	_terMgr->drawTerrains(true,true,false,vGroundAmbient);

	_skyFBO->End();
}

void MainScene::render()
{

	Sky &sky = Sky::getInstance();
	GUI &gui = GUI::getInstance();
	Camera& cam = Camera::getInstance();
	Frustum::getInstance().Extract(cam.getEye());

	updateLights();

	sky.setParams(Camera::getInstance().getEye(),vec3(_sunVector),false,true,true);
	sky.draw();


	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, _white/6);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, _white);

	renderActors();

	vec4 vGroundAmbient = _white.lerp(_white*0.2f, _black, _sun_cosy);

	_terMgr->drawTerrains(true,false,false,vGroundAmbient);
	_terMgr->drawInfinitePlane(2.0f*4500,*_skyFBO);

	gui.draw();	
	
}

void MainScene::renderActors()
{
	GFXDevice::getInstance().renderElements(ModelArray);
	GFXDevice::getInstance().renderElements(GeometryArray);

}

void MainScene::processInput()
{
	moveFB  = Engine::getInstance().moveFB;
	moveLR  = Engine::getInstance().moveLR;
	angleLR = Engine::getInstance().angleLR;
	angleUD = Engine::getInstance().angleUD;
	
	if(angleLR)	Camera::getInstance().RotateX(angleLR * Framerate::getInstance().getSpeedfactor()/5);
	if(angleUD)	Camera::getInstance().RotateY(angleUD * Framerate::getInstance().getSpeedfactor()/5);
	if(moveFB)	Camera::getInstance().PlayerMoveForward(moveFB * Framerate::getInstance().getSpeedfactor());
	if(moveLR)	Camera::getInstance().PlayerMoveStrafe(moveLR * Framerate::getInstance().getSpeedfactor());

}


void MainScene::processEvents(F32 time)
{
	F32 SunDisplay = 1.10f;
	F32 FpsDisplay = 0.3f;
	F32 TimeDisplay = 0.01f;
	if (time - _eventTimers[0] >= SunDisplay)
	{
		_sunAngle.y += 0.0005f;
		_sunVector = vec4(	-cosf(_sunAngle.x) * sinf(_sunAngle.y),
							-cosf(_sunAngle.y),
							-sinf(_sunAngle.x) * sinf(_sunAngle.y),
							0.0f );
		_eventTimers[0] += SunDisplay;
	}

	if (time - _eventTimers[1] >= FpsDisplay)
	{
		
		GUI::getInstance().modifyText("fpsDisplay", "FPS: %5.2f", Framerate::getInstance().getFps());
		_eventTimers[1] += FpsDisplay;
	}
    
	
	if (time - _eventTimers[2] >= TimeDisplay)
	{
		GUI::getInstance().modifyText("timeDisplay", "Elapsed time: %5.0f", time);
		_eventTimers[2] += TimeDisplay;
	}
	if(PhysX::getInstance().getScene() != NULL)	PhysX::getInstance().UpdateActors();
}

bool MainScene::load(const string& name)
{
	bool state = false;
	addDefaultLight();
	_terMgr->createTerrains(TerrainInfoArray);
	state = loadResources(true);	
	state = loadEvents(true);
	return state;
}

bool _switchAB = false;
void MainScene::test(boost::any a, CallbackParam b)
{
	boost::mutex::scoped_lock l(_mutex);
	vec3 pos = ModelArray["box"]->getTransform()->getPosition();
 
	if(!_switchAB)
	{
		if(pos.x < 300 && pos.z == 0)		   pos.x++;
		if(pos.x == 300)
		{
			if(pos.y < 800 && pos.z == 0)      pos.y++;
			if(pos.y == 800)
			{
				if(pos.z > -500)   pos.z--;
				if(pos.z == -500)  _switchAB = true;
			}
			
		}
	}
	else 
	{
		if(pos.x > -300 && pos.z ==  -500)      pos.x--;
		if(pos.x == -300)
		{
			if(pos.y > 100 && pos.z == -500)    pos.y--;
			if(pos.y == 100)
			{
				if(pos.z < 0)    pos.z++;
				if(pos.z == 0)   _switchAB = false;
			}
		}
	}

	ModelArray["box"]->getTransform()->setPosition(pos);
}

bool MainScene::loadResources(bool continueOnErrors)
{
	GUI& gui = GUI::getInstance();

	angleLR=0.0f,angleUD=0.0f,moveFB=0.0f;
	gui.addText("fpsDisplay",           //Unique ID
		                       vec3(60,60,0),          //Position
							   BITMAP_8_BY_13,    //Font
							   vec3(0.0f,0.2f, 1.0f),  //Color
							   "HELLO! FPS: %s",0);    //Text and arguments

	gui.addText("timeDisplay",
								vec3(60,70,0),
								BITMAP_8_BY_13,
								vec3(0.6f,0.2f,0.2f),
								"Elapsed time: %5.0f",GETTIME());
	_eventTimers.push_back(0.0f); //Sun
	_eventTimers.push_back(0.0f); //Fps
	_eventTimers.push_back(0.0f); //Time
	_skyFBO = GFXDevice::getInstance().newFBO();
	_depthMap[0] = GFXDevice::getInstance().newFBO();
	_depthMap[1] = GFXDevice::getInstance().newFBO();

	_skyFBO->Create(FrameBufferObject::FBO_2D_COLOR, 1024, 1024);
	_sunAngle = vec2(0.0f, RADIANS(45.0f));
	_sunVector = vec4(	-cosf(_sunAngle.x) * sinf(_sunAngle.y),
							-cosf(_sunAngle.y),
							-sinf(_sunAngle.x) * sinf(_sunAngle.y),
							0.0f );


	_events.push_back(new Event(30,true,false,boost::bind(&MainScene::test,this,string("test"),TYPE_STRING)));


	return true;
}