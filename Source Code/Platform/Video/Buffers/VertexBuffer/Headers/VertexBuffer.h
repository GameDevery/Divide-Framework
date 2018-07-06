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

#ifndef _VERTEX_BUFFER_OBJECT_H
#define _VERTEX_BUFFER_OBJECT_H

#include "VertexDataInterface.h"

namespace Divide {

class ShaderProgram;
/// Vertex Buffer interface class to allow API-independent implementation of
/// data
/// This class does NOT represent an API-level VB, such as: GL_ARRAY_BUFFER /
/// D3DVERTEXBUFFER
/// It is only a "buffer" for "vertex info" abstract of implementation. (e.g.:
/// OGL uses a vertex array object for this)

class NOINITVTABLE VertexBuffer : public VertexDataInterface
{
   protected:
    enum class VertexAttribute : U32 {
        ATTRIB_POSITION = 0,
        ATTRIB_COLOR    = 1,
        ATTRIB_NORMAL   = 2,
        ATTRIB_TEXCOORD = 3,
        ATTRIB_TANGENT  = 4,
        ATTRIB_BONE_WEIGHT = 5,
        ATTRIB_BONE_INDICE = 6,
        COUNT = 7
    };

   public:
       
    struct Vertex {
        vec3<F32> _position;
        F32       _normal;
        F32       _tangent;
        vec4<U8>  _color;
        vec2<F32> _texcoord;
        vec4<F32> _weights;
        P32       _indices;
    };

    VertexBuffer()
        : VertexDataInterface(),
          _LODcount(0),
          _format(GFXDataFormat::UNSIGNED_SHORT),
          _indexDelimiter(0),
          _primitiveRestartEnabled(false),
          _created(false),
          _currentPartitionIndex(0),
          _largeIndices(false)
    {
        reset();
    }

    virtual ~VertexBuffer()
    {
        _LODcount = 1;
        reset();
    }

    virtual bool create(bool staticDraw = true) = 0;
    virtual void destroy() = 0;
    /// Some engine elements, like physics or some geometry shading techniques
    /// require a triangle list.
    virtual bool setActive() = 0;

    virtual void draw(const GenericDrawCommand& command,
                      bool useCmdBuffer = false) = 0;

    inline void setLODCount(const U8 LODcount) { _LODcount = LODcount; }

    inline void useLargeIndices(bool state = true) {
        DIVIDE_ASSERT(!_created,
                      "VertexBuffer error: Index format type specified before "
                      "buffer creation!");
        _largeIndices = state;
        _format = state ? GFXDataFormat::UNSIGNED_INT
                        : GFXDataFormat::UNSIGNED_SHORT;
    }

    inline void setVertexCount(U32 size) { _data.resize(size); }

    inline size_t getVertexCount() const {
        return _data.size();
    }

    inline const vectorImpl<Vertex>& getVertices() const {
        return _data;
    }

    inline void reserveIndexCount(U32 size) {
        usesLargeIndices() ? _hardwareIndicesL.reserve(size)
                           : _hardwareIndicesS.reserve(size);
    }

    inline void resizeVertexCount(U32 size, const Vertex& defaultValue = Vertex()) {
        _data.resize(size, defaultValue);
    }

    inline const vec3<F32>& getPosition(U32 index) const {
        return _data[index]._position;
    }

    inline const vec2<F32>& getTexCoord(U32 index) const {
        return _data[index]._texcoord;
    }

    inline F32 getNormal(U32 index) const {
        return _data[index]._normal;
    }

    inline F32 getNormal(U32 index, vec3<F32>& normalOut) const {
        F32 normal = getNormal(index);
        Util::UNPACK_VEC3(normal, normalOut.x, normalOut.y, normalOut.z);
        return normal;
    }

    inline F32 getTangent(U32 index) const {
        return _data[index]._tangent;
    }

    inline F32 getTangent(U32 index, vec3<F32>& tangentOut) const {
        F32 tangent = getTangent(index);
        Util::UNPACK_VEC3(tangent, tangentOut.x, tangentOut.y, tangentOut.z);
        return tangent;
    }

    inline P32 getBoneIndices(U32 index) const {
        return _data[index]._indices;
    }

    inline const vec4<F32>& getBoneWeights(U32 index) const {
        return _data[index]._weights;
    }

    virtual bool queueRefresh() = 0;

    inline bool usesLargeIndices() const { return _largeIndices; }

    inline U32 getIndexCount() const {
        return to_uint(usesLargeIndices() ? _hardwareIndicesL.size()
                                          : _hardwareIndicesS.size());
    }

    inline U32 getIndex(U32 index) const {
        assert(index < getIndexCount());

        return usesLargeIndices() ? _hardwareIndicesL[index]
                                  : _hardwareIndicesS[index];
    }

    template<typename T>
    const vectorImpl<T>& getIndices() const;/* {
        static_assert(false,
                "VertexBuffer::getIndices error: Need valid index data type!");
    }*/

    template <typename T>
    inline void addIndex(T index) {
        if (usesLargeIndices()) {
            _hardwareIndicesL.push_back(to_uint(index));
        } else {
            _hardwareIndicesS.push_back(to_ushort(index));
        }
    }

    inline void addRestartIndex() {
        _primitiveRestartEnabled = true;
        if (usesLargeIndices()) {
        	addIndex(Config::PRIMITIVE_RESTART_INDEX_L);
        } else {
            addIndex(Config::PRIMITIVE_RESTART_INDEX_S);
        }
    }

    inline void modifyPositionValue(U32 index, const vec3<F32>& newValue) {
        modifyPositionValue(index, newValue.x, newValue.y, newValue.z);
    }

    inline void modifyPositionValue(U32 index, F32 x, F32 y, F32 z) {
        assert(index < _data.size());

        _data[index]._position.set(x, y, z);
        _attribDirty[to_uint(VertexAttribute::ATTRIB_POSITION)] = true;
    }

    inline void modifyColorValue(U32 index, const vec4<U8>& newValue) {
        modifyColorValue(index, newValue.r, newValue.g, newValue.b, newValue.a);
    }

    inline void modifyColorValue(U32 index, U8 r, U8 g, U8 b, U8 a) {
        assert(index < _data.size());

        _data[index]._color.set(r, g, b, a);
        _attribDirty[to_uint(VertexAttribute::ATTRIB_COLOR)] = true;
    }

    inline void modifyNormalValue(U32 index, const vec3<F32>& newValue) {
        modifyNormalValue(index, newValue.x, newValue.y, newValue.z);
    }

    inline void modifyNormalValue(U32 index, F32 x, F32 y, F32 z) {
        assert(index < _data.size());

        _data[index]._normal = Util::PACK_VEC3(x, y, z);
        _attribDirty[to_uint(VertexAttribute::ATTRIB_NORMAL)] = true;
    }

    inline void modifyTangentValue(U32 index, const vec3<F32>& newValue) {
        modifyTangentValue(index, newValue.x, newValue.y, newValue.z);
    }

    inline void modifyTangentValue(U32 index, F32 x, F32 y, F32 z) {
        assert(index < _data.size());

        _data[index]._tangent = Util::PACK_VEC3(x, y, z);
        _attribDirty[to_uint(VertexAttribute::ATTRIB_TANGENT)] = true;
    }

    inline void modifyTexCoordValue(U32 index, const vec2<F32>& newValue) {
        modifyTexCoordValue(index, newValue.s, newValue.t);
    }

    inline void modifyTexCoordValue(U32 index, F32 s, F32 t) {
        assert(index < _data.size());

        _data[index]._texcoord.set(s, t);
        _attribDirty[to_uint(VertexAttribute::ATTRIB_TEXCOORD)] = true;
    }

    inline void modifyBoneIndices(U32 index, P32 indices) {
        assert(index < _data.size());

        _data[index]._indices = indices;
        _attribDirty[to_uint(VertexAttribute::ATTRIB_BONE_INDICE)] = true;
    }

    inline void modifyBoneWeights(U32 index, const vec4<F32>& weights) {
        assert(index < _data.size());

        _data[index]._weights = weights;
        _attribDirty[to_uint(VertexAttribute::ATTRIB_BONE_WEIGHT)] = true;
    }

    inline void shrinkAllDataToFit() {
        shrinkToFit(_data);
    }

    inline size_t partitionBuffer() {
        U32 currentIndexCount = getIndexCount();

        _partitions.push_back(std::make_pair(
            getIndexCount() - currentIndexCount, currentIndexCount));
        _currentPartitionIndex = to_uint(_partitions.size());
        return _currentPartitionIndex - 1;
    }

    inline U32 getPartitionCount(U16 partitionID) {
        if (_partitions.empty()) {
            return getIndexCount();
        }
        DIVIDE_ASSERT(partitionID < _partitions.size(),
                      "VertexBuffer error: Invalid partition offset!");
        return _partitions[partitionID].second;
    }

    inline U32 getPartitionOffset(U16 partitionID) {
        if (_partitions.empty()) {
            return 0;
        }
        DIVIDE_ASSERT(partitionID < _partitions.size(),
                      "VertexBuffer error: Invalid partition offset!");
        return _partitions[partitionID].first;
    }

    inline U32 getLastPartitionOffset() {
        if (_partitions.empty()) {
            return 0;
        }
        if (_partitions.empty()) return 0;
        return getPartitionOffset(to_ushort(_partitions.size() - 1));
    }

    inline void reset() {
        _created = false;
        _primitiveRestartEnabled = false;
        _partitions.clear();
        _data.clear();
        _hardwareIndicesL.clear();
        _hardwareIndicesS.clear();
        _attribDirty.fill(false);
    }

   protected:
    /// Just before we render the frame
    virtual bool frameStarted(const FrameEvent& evt) {
        return VertexDataInterface::frameStarted(evt);
    }

    virtual void checkStatus() = 0;
    virtual bool refresh() = 0;
    virtual bool createInternal() = 0;

   protected:
    /// Number of LOD nodes in this buffer
    U8 _LODcount;
    /// The format of the buffer data
    GFXDataFormat _format;
    /// An index value that separates objects (OGL: primitive restart index)
    U32 _indexDelimiter;
    // first: offset, second: count
    vectorImpl<std::pair<U32, U32> > _partitions;
    /// Used for creating an "IB". If it's empty, then an outside source should
    /// provide the indices
    vectorImpl<U32> _hardwareIndicesL;
    vectorImpl<U16> _hardwareIndicesS;
    vectorImpl<Vertex> _data;
    /// Cache system to update only required data
    std::array<bool, to_const_uint(VertexAttribute::COUNT)> _attribDirty;
    bool _primitiveRestartEnabled;
    /// Was the data submitted to the GPU?
    bool _created;

   private:
    U32 _currentPartitionIndex;
    /// Use either U32 or U16 indices. Always prefer the later
    bool _largeIndices;
};

template<>
inline const vectorImpl<U32>& VertexBuffer::getIndices<U32>() const {
    return _hardwareIndicesL;
}

template<>
inline const vectorImpl<U16>& VertexBuffer::getIndices<U16>() const {
    return _hardwareIndicesS;
}
};  // namespace Divide
#endif
