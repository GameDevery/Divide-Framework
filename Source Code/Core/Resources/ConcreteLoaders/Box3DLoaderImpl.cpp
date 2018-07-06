#include "Core/Resources/Headers/ResourceLoader.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Geometry/Shapes/Headers/Predefined/Box3D.h"

namespace Divide {

template<>
Box3D* ImplResourceLoader<Box3D>::operator()() {
    F32 size = 1.0f;
    if ( !_descriptor.getPropertyListString().empty() ) {
        size = atof( _descriptor.getPropertyListString().c_str() );//<should work
    }

    Box3D* ptr = New Box3D( size );

    if ( !load( ptr, _descriptor.getName() ) ) {
        MemoryManager::SAFE_DELETE( ptr );
    } else {
        if ( _descriptor.getFlag() ) {
            ptr->renderState().useDefaultMaterial( false );
        }
    }

    return ptr;
}

DEFAULT_LOADER_IMPL(Box3D)

};