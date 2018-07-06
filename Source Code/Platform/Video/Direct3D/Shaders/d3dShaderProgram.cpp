#include "stdafx.h"

#include "Headers/d3dShaderProgram.h"

namespace Divide {

IMPLEMENT_CUSTOM_ALLOCATOR(d3dShaderProgram, 0, 0);
d3dShaderProgram::d3dShaderProgram(GFXDevice& context, size_t descriptorHash, const stringImpl& name, const stringImpl& resourceName, const stringImpl& resourceLocation, bool asyncLoad) :
    ShaderProgram(context, descriptorHash, name, resourceName, resourceLocation, asyncLoad)
{
}

d3dShaderProgram::~d3dShaderProgram()
{
}

bool d3dShaderProgram::unload() {
    return ShaderProgram::unload();
}

bool d3dShaderProgram::recompileInternal() {
    return true;
}

bool d3dShaderProgram::isValid() const {
    return false;
}

// Subroutines
U32 d3dShaderProgram::GetSubroutineIndex(ShaderType type, const char* name) const {
    return 0;
}

U32 d3dShaderProgram::GetSubroutineUniformLocation(ShaderType type, const char* name) const {
    return 0;
}

U32 d3dShaderProgram::GetSubroutineUniformCount(ShaderType type) const {
    return 0;
}

// Uniforms
I32 d3dShaderProgram::Binding(const char* name) {
    return -1;
}

bool d3dShaderProgram::load(const DELEGATE_CBK<void, CachedResource_wptr>& onLoadCallback) {
    return ShaderProgram::load(onLoadCallback);
}

};
