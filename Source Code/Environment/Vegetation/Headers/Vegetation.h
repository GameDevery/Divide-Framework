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

#ifndef VEGETATION_H_
#define VEGETATION_H_

#include "Utility/Headers/ImageTools.h"

class VertexBufferObject;
class Terrain;
class ShaderProgram;
class Texture;
typedef Texture Texture2D;
class FrameBufferObject;
class SceneGraphNode;

class Vegetation{
public:
	Vegetation(U16 billboardCount, D32 grassDensity, F32 grassScale, D32 treeDensity, F32 treeScale, const std::string& map, std::vector<Texture2D*>& grassBillboards): 
	  _billboardCount(billboardCount),
	  _grassDensity(grassDensity),
	  _grassScale(grassScale),
	  _treeScale(treeScale),
	  _treeDensity(treeDensity),
      _grassBillboards(grassBillboards),
	  _render(false),
	  _success(false),
	  _terrain(NULL),
	  _grassShader(NULL){
		  ImageTools::OpenImage(map,_map);
	  }
	~Vegetation();
	void initialize(const std::string& grassShader, const std::string& terrainName);
	inline void toggleRendering(bool state){_render = state;}
	void draw(bool drawInReflection);

private:
	//variables
	bool _render, _success ;                      //Toggle vegetation rendering On/Off
	SceneGraphNode* _terrain;
	D32 _grassDensity, _treeDensity;
	U16 _billboardCount;          //Vegetation cumulated density
	F32 _grassSize,_grassScale, _treeScale;
	F32 _windX, _windZ, _windS, _time;
	ImageTools::ImageData _map;  //Dispersion map for vegetation placement
	std::vector<Texture2D*>	_grassBillboards;
	ShaderProgram*		    _grassShader;

	bool generateTrees();			   //True = Everything OK, False = Error. Check _errorCode
	bool generateGrass(U32 index);     //index = current grass type (billboard, vbo etc)

	std::vector<VertexBufferObject*>	_grassVBO;
};

#endif