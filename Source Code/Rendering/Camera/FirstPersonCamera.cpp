#include "stdafx.h"

#include "Headers/FirstPersonCamera.h"

namespace Divide {

FirstPersonCamera::FirstPersonCamera(const Str128& name, const vec3<F32>& eye)
    : FreeFlyCamera(name, Type(), eye)
{
}

};