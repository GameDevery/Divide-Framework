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

#ifndef _WRAPPER_GL_H_
#define _WRAPPER_GL_H_


#include "EngineGraphs/SceneNode.h"
#include "../RenderAPIWrapper.h"

#include "glFrameBufferObject.h"
#include "glVertexBufferObject.h"
#include "glPixelBufferObject.h"
#include "glShader.h"
#include "glTexture.h"
#define CHECK_OPENGL_ERROR( cmd ) \
	cmd; \
	 { GLenum  error; \
		 while ( (error = glGetError()) != GL_NO_ERROR) { \
			 Console::getInstance().errorfn( "[%s:%d] '%s' failed with error %s", __FILE__, __LINE__, #cmd, gluErrorString(error) ); \
		 }}

DEFINE_SINGLETON_EXT1(GL_API,RenderAPIWrapper)
	typedef unordered_map<std::string, SceneGraphNode*> sceneGraphMap;
private:
	GL_API() : RenderAPIWrapper(), _windowId(0) {}

	void initHardware();
	void closeRenderingApi();
	void initDevice();
	void resizeWindow(U16 w, U16 h);
	void lookAt(const vec3& eye,const vec3& center,const vec3& up, bool invertx = false, bool inverty = false);
	void idle();

    void getModelViewMatrix(mat4& mvMat);
	void getProjectionMatrix(mat4& projMat);

	FrameBufferObject*  newFBO(){return New glFrameBufferObject(); }
	VertexBufferObject* newVBO(){return New glVertexBufferObject(); }
	PixelBufferObject*  newPBO(){return New glPixelBufferObject(); }

	Texture2D*          newTexture2D(bool flipped = false){return New glTexture(0x0DE1/*GL_TEXTURE_2D*/,flipped);}
	TextureCubemap*     newTextureCubemap(bool flipped = false){return New glTexture(0x8513/*GL_TEXTURE_CUBEMAP*/,flipped);}

	Shader* newShader(const char *vsFile, const char *fsFile){return New glShader(vsFile,fsFile); }
	Shader* newShader(){return New glShader(); }

	typedef void (*callback)();	void glCommand(callback f){f();}

	void clearBuffers(U8 buffer_mask);
	void swapBuffers();
	void enableFog(F32 density, F32* color);

	void toggle2D(bool _2D);
	void setOrthoProjection(const vec4& rect, const vec2& planes);

	void drawTextToScreen(Text*);
	void drawCharacterToScreen(void* ,char);
	void drawButton(Button*);
	void drawFlash(GuiFlash* flash);

	void drawBox3D(vec3 min, vec3 max);
	void drawBox3D(Box3D* const box);
	void drawSphere3D(Sphere3D* const sphere);
	void drawQuad3D(Quad3D* const quad);
	void drawText3D(Text3D* const text);
    void drawBox3D(SceneGraphNode* node);
	void drawSphere3D(SceneGraphNode* node);
	void drawQuad3D(SceneGraphNode* node);
	void drawText3D(SceneGraphNode* node);

	void renderModel(SceneGraphNode* node);
	void renderElements(Type t, Format f, U32 count, const void* first_element);
	
	void setMaterial(Material* mat);

	void setLight(U8 slot, unordered_map<std::string,vec4>& properties);
	void createLight(U8 slot);
	void setLightCameraMatrices(const vec3& lightPosVector, const vec3& lightTargetVector,bool directional = false);
	void restoreLightCameraMatrices(bool directional = false);

	void toggleWireframe(bool state);

	void Screenshot(char *filename, U16 xmin, U16 ymin, U16 xmax, U16 ymax);

	void setRenderState(RenderState& state);
	void ignoreStateChanges(bool state);

	void setDepthMapRendering(bool state);
private: //OpenGL specific:

	void setObjectState(Transform* const transform);
	void releaseObjectState();
	U8 _windowId;

END_SINGLETON

#endif
