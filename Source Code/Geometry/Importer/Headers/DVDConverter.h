/*
   Copyright (c) 2015 DIVIDE-Studio
   Copyright (c) 2009 Ionut Cava

   This file is part of DIVIDE Framework.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software
   and associated documentation files (the "Software"), to deal in the Software
   without restriction,
   including without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
   PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
   IN CONNECTION WITH THE SOFTWARE
   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

/*
Copyright (c) 2006-2012 assimp team
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
    Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.
    Neither the name of the assimp team nor the names of its contributors may be
used to endorse or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL,SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS
OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _DIVIDE_FORMAT_CONVERTER_H_
#define _DIVIDE_FORMAT_CONVERTER_H_

#include "Geometry/Material/Headers/Material.h"
#include "Platform/Video/Textures/Headers/Texture.h"

struct aiScene;
struct aiMesh;
struct aiMaterial;
struct aiNode;

namespace Assimp {
class Importer;
};

namespace Divide {

class Mesh;
class SubMesh;
DEFINE_SINGLETON(DVDConverter)
  public:
    Mesh* load(const stringImpl& file);
    bool init();

  private:
    DVDConverter();
    ~DVDConverter();

    SubMesh* loadSubMeshGeometry(const aiMesh* source, Mesh* parentMesh,
                                 U16 count,
                                 U8& submeshBoneOffsetOut,
                                 U32& previousVertOffset);

    Material* loadSubMeshMaterial(bool skinned,
                                  const aiMaterial* source,
                                  const stringImpl& materialName);

  private:
    struct vertexWeight {
        U8 _boneID;
        F32 _boneWeight;
        vertexWeight(U8 ID, F32 weight) : _boneID(ID), _boneWeight(weight) {}
    };

    Assimp::Importer* importer;

    const aiScene* _aiScenePointer;
    U32 _ppsteps;
    stringImpl _fileLocation;
    stringImpl _modelName;

    hashMapImpl<U32, Material::ShadingMode> aiShadingModeInternalTable;
    hashMapImpl<U32, TextureWrap> aiTextureMapModeTable;
    hashMapImpl<U32, Material::TextureOperation> aiTextureOperationTable;
END_SINGLETON

};  // namespace Divide

#endif
