/*�Copyright 2009-2012 DIVIDE-Studio�*/
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

#ifndef _SHADER_HANDLER_H_
#define _SHADER_HANDLER_H_

#include "Core/Resources/Headers/Resource.h"
#include <assimp/anim.h>
class Shader;
enum SHADER_TYPE;
class ShaderProgram : public Resource {

public:
	virtual bool load(const std::string& name);
	
	virtual void bind();
	virtual void unbind();
	
	inline U32 getId() { return _shaderProgramId; }

	virtual void attachShader(Shader* shader) = 0;
	std::vector<Shader* > getShaders(SHADER_TYPE type);
	///Attributes
	virtual void Attribute(const std::string& ext, D32 value) = 0;
	virtual void Attribute(const std::string& ext, F32 value) = 0 ;
	virtual void Attribute(const std::string& ext, const vec2<F32>& value) = 0;
	virtual void Attribute(const std::string& ext, const vec3<F32>& value) = 0;
	virtual void Attribute(const std::string& ext, const vec4<F32>& value) = 0;
	///Uniforms
	virtual void Uniform(const std::string& ext, I32 value) = 0;
	virtual void Uniform(const std::string& ext, F32 value) = 0 ;
	virtual void Uniform(const std::string& ext, const vec2<F32>& value) = 0;
	virtual void Uniform(const std::string& ext, const vec3<F32>& value) = 0;
	virtual void Uniform(const std::string& ext, const vec4<F32>& value) = 0;
	virtual void Uniform(const std::string& ext, const mat3<F32>& value, bool rowMajor = false) = 0;
	virtual void Uniform(const std::string& ext, const mat4<F32>& value, bool rowMajor = false) = 0;
	virtual void Uniform(const std::string& ext, const std::vector<mat4<F32> >& values, bool rowMajor = false) = 0;
	///Uniform Texture
	virtual void UniformTexture(const std::string& ext, U16 slot) = 0;

	virtual I32 getAttributeLocation(const std::string& name) = 0;
	virtual I32 getUniformLocation(const std::string& name) = 0;

	virtual ~ShaderProgram();

	inline void commit() {if(!_compiled) {link(); validate();}}

	inline bool isBound() {return _bound;}
protected:
	virtual void validate() = 0;
	virtual void link() = 0;
	ShaderProgram() : Resource(), _compiled(false) {}
	
	static bool checkBinding(U32 newShaderProgramId);
protected:
	bool _compiled;
	bool _bound;
	U32 _shaderProgramId;
	std::vector<Shader* > _shaders;
	static U32 _prevShaderProgramId;
	static U32 _newShaderProgramId;
};


#endif
