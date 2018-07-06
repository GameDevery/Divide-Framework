#ifndef _CUBE_SCENE_H
#define _CUBE_SCENE_H

#include "Scene.h"

class CubeScene : public Scene
{
public:
	void render();
	void preRender();
	bool load(const string& name);
	bool loadResources(bool continueOnErrors);
	bool loadEvents(bool continueOnErrors){return true;}
	bool unload();
	void processInput();
	void processEvents(F32 time){}

private:
	F32 i ;
	vec2 _sunAngle;
	vec4 _sunVector,_white,_black;
	F32 angleLR,angleUD,moveFB,moveLR,update_time,_sun_cosy;
	Box3D *_box;
	Text3D *_text3D;
	Quad3D *_innerQuad, *_outterQuad;
	vec4 _vSunColor;
};

#endif