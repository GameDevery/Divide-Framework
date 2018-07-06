/*�Copyright 2009-2013 DIVIDE-Studio�*/
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


#ifndef D3D_SHADER_H_
#define D3D_SHADER_H_

#include "Hardware/Video/Shaders/Headers/Shader.h"

class d3dShader : public Shader{
public:
	d3dShader(const std::string& name, ShaderType type) : Shader(name,type) {}
	~d3dShader(){}

	bool load(const std::string& source){return true;}
	void validate(){}
};

#endif