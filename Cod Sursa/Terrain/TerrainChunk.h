#ifndef TERRAINCHUNK_H
#define TERRAINCHUNK_H

#include "resource.h"
#include "Importer/DVDConverter.h"

#define TERRAIN_CHUNKS_LOD 3

#define TERRAIN_CHUNK_LOD0	100.0f
#define TERRAIN_CHUNK_LOD1	180.0f

class TerrainChunk
{
public:
	void Destroy();
	int  DrawGround(U32 lod);
	void DrawGrass(U32 lod, F32 d,bool drawDepthMap);
	void DrawTrees(U32 lod, F32 d,bool drawDepthMap);
	int  DrawObjects(U32 lod,bool drawDepthMap);
	void Load(U32 depth, ivec2 pos, ivec2 HMsize);

	inline vector<U32>&				getIndiceArray(U32 lod)	{return m_tIndice[lod];}
	inline vector<U32>&				getGrassIndiceArray()		{return m_tGrassIndice;}
	inline vector<DVDFile*>&	   	getObjectsArray()			{return m_tObject;}
	inline vector<DVDFile >&        getTreeArray()              {return m_tTrees;}
	void								addObject(DVDFile* obj);
	void								addTree(vec3 pos, F32 rotation, F32 scale);
	TerrainChunk() {}
	~TerrainChunk() {Destroy();}

private:
	void ComputeIndicesArray(U32 lod, U32 depth, ivec2 pos, ivec2 HMsize);
	void loadTrees();

private:
	vector<U32>  		m_tIndice[TERRAIN_CHUNKS_LOD];
	U32					m_tIndOffsetW[TERRAIN_CHUNKS_LOD];
	U32					m_tIndOffsetH[TERRAIN_CHUNKS_LOD];

	vector<U32>	        m_tGrassIndice;

	//ToDo: Eliminate this hack. Trees hold a pointer to an Object3D object that represents the tree's geometry,
	//      and has a position, orientation and scale different from that of the object itself.
	vector<DVDFile >           m_tTrees;
	vector<DVDFile >::iterator m_tTreesIterator;

	vector<DVDFile*>	    m_tObject;
	string				    previousModel;
};

#endif

