#ifndef _QUAD_3D_H_
#define _QUAD_3D_H_
#include "Geometry/Object3D.h"

class Shader;
class Quad3D : public Object3D
{
public:
	Quad3D(const vec3& topLeft,const  vec3& topRight,
		   const  vec3& bottomLeft,const  vec3& bottomRight) : _tl(topLeft),
		    												   _tr(topRight),
															   _bl(bottomLeft),
															   _br(bottomRight),
															   _texture(NULL),
															   _shader(NULL)
															   {_geometryType = QUAD_3D;}
	bool load(const std::string &name) {return true;}
	bool unload() {return true;}

	inline Texture2D* getTexture() {return _texture;}
	inline Shader*    getShader()  {return _shader;}
	vec3 _tl,_tr,_bl,_br; //ToDo: hack. Add proper accessors - Ionut
private:
	Texture2D* _texture;
	Shader*    _shader;
};


#endif