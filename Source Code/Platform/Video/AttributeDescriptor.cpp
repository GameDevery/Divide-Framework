#include "stdafx.h"

#include "Headers/AttributeDescriptor.h"

namespace Divide
{
    [[nodiscard]] size_t GetHash( const AttributeDescriptor& descriptor )
    {
        if ( descriptor._dataType == GFXDataFormat::COUNT )
        {
            return 0u;
        }

        size_t hash = 1337;
        Util::Hash_combine( hash, descriptor._strideInBytes, descriptor._vertexBindingIndex,
                            descriptor._componentsPerElement,
                            descriptor._dataType, descriptor._normalized );

        return hash;
    }

    [[nodiscard]] size_t GetHash( const VertexBinding& vertexBinding )
    {
        size_t hash = 1337;
        Util::Hash_combine( hash, vertexBinding._strideInBytes, vertexBinding._bufferBindIndex, vertexBinding._perVertexInputRate);
        return hash;
    }

    [[nodiscard]] size_t GetHash( const AttributeMap& attributes )
    {
        size_t vertexFormatHash = 1337;
        for ( const AttributeDescriptor& attrDescriptor : attributes._attributes )
        {
            if ( attrDescriptor._dataType != GFXDataFormat::COUNT )
            {
                Util::Hash_combine( vertexFormatHash, GetHash( attrDescriptor ) );
            }
        }
        for ( const VertexBinding& vertBinding : attributes._vertexBindings )
        {
            Util::Hash_combine( vertexFormatHash, GetHash( vertBinding ) );
        }

        return vertexFormatHash;
    }

    bool operator==( const AttributeDescriptor& lhs, const AttributeDescriptor& rhs ) noexcept
    {
        return lhs._strideInBytes == rhs._strideInBytes &&
               lhs._vertexBindingIndex == rhs._vertexBindingIndex &&
               lhs._componentsPerElement == rhs._componentsPerElement &&
               lhs._dataType == rhs._dataType &&
               lhs._normalized == rhs._normalized;
    }

    bool operator!=( const AttributeDescriptor& lhs, const AttributeDescriptor& rhs ) noexcept
    {
        return lhs._strideInBytes != rhs._strideInBytes ||
               lhs._vertexBindingIndex != rhs._vertexBindingIndex ||
               lhs._componentsPerElement != rhs._componentsPerElement ||
               lhs._dataType != rhs._dataType ||
               lhs._normalized != rhs._normalized;
    }

    bool operator==( const VertexBinding& lhs, const VertexBinding& rhs ) noexcept
    {
        return lhs._perVertexInputRate == rhs._perVertexInputRate &&
               lhs._bufferBindIndex == rhs._bufferBindIndex &&
               lhs._strideInBytes == rhs._strideInBytes;
    }

    bool operator!=( const VertexBinding& lhs, const VertexBinding& rhs ) noexcept
    {
        return lhs._perVertexInputRate != rhs._perVertexInputRate ||
               lhs._bufferBindIndex != rhs._bufferBindIndex ||
               lhs._strideInBytes != rhs._strideInBytes;
    }

    bool operator==( const AttributeMap& lhs, const AttributeMap& rhs ) noexcept
    {
        return lhs._vertexBindings == rhs._vertexBindings &&
               lhs._attributes == rhs._attributes;
    }

    bool operator!=( const AttributeMap& lhs, const AttributeMap& rhs ) noexcept
    {
        return lhs._vertexBindings != rhs._vertexBindings ||
               lhs._attributes != rhs._attributes;
    }
}; //namespace Divide