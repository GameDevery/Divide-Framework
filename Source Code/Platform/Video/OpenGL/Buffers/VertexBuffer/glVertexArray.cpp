#include "Headers/glVertexArray.h"

#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Shaders/Headers/ShaderManager.h"

#include "Core/Headers/Console.h"
#include "Utility/Headers/Localization.h"

namespace Divide {

/// Default destructor
glVertexArray::glVertexArray() : VertexBuffer(),
                                _refreshQueued(false),
                                _animationData(false),
                                _formatInternal(GL_NONE)
{
    //We assume everything is static draw
    _usage = GL_STATIC_DRAW;
    memset(_prevSize, -1, VertexAttribute_PLACEHOLDER * sizeof(GLsizei));
    _prevSizeIndices = -1;
    _VAOid = _VBid = _IBid = 0;
}

glVertexArray::~glVertexArray()
{
    Destroy();
}

/// Delete buffer
void glVertexArray::Destroy(){
    if(_IBid > 0)  glDeleteBuffers(1, &_IBid);
    if(_VAOid > 0) glDeleteVertexArrays(1, &_VAOid);
    if(_VBid > 0)  glDeleteBuffers(1, &_VBid);
    _VAOid = _VBid = _IBid = 0;
}

/// Create a dynamic or static VB
bool glVertexArray::Create(bool staticDraw){
    //If we want a dynamic buffer, then we are doing something outdated, such as software skinning, or software water rendering
    if(!staticDraw){
        //OpenGLES support isn't added, but this check doesn't break anything, so I'll just leave it here -Ionut
        GLenum usage = (GFX_DEVICE.getApi() == OpenGLES) ? GL_STREAM_DRAW : GL_DYNAMIC_DRAW;
        if(usage != _usage){
            _usage = usage;
            _refreshQueued = true;
        }
    }

    return true;
}

/// Update internal data based on construction method (dynamic/static)
bool glVertexArray::Refresh(){
    // Dynamic LOD elements (such as terrain) need dynamic indices
    // We can manually override index usage (again, used by the Terrain rendering system)
    DIVIDE_ASSERT(!_hardwareIndicesL.empty() || !_hardwareIndicesS.empty(), 
                  "glVertexArray error: Invalid index data on Refresh()!");
    // Get the size of each data container
    GLsizei nSize[VertexAttribute_PLACEHOLDER];

    nSize[ATTRIB_POSITION]    = (GLsizei)(_dataPosition.size()  * sizeof(vec3<GLfloat>));
    nSize[ATTRIB_COLOR]       = (GLsizei)(_dataColor.size()     * sizeof(vec3<GLubyte>));
    nSize[ATTRIB_NORMAL]      = (GLsizei)(_dataNormal.size()    * sizeof(vec3<GLfloat>));
    nSize[ATTRIB_TEXCOORD]    = (GLsizei)(_dataTexcoord.size()  * sizeof(vec2<GLfloat>));
    nSize[ATTRIB_TANGENT]     = (GLsizei)(_dataTangent.size()   * sizeof(vec3<GLfloat>));
    nSize[ATTRIB_BITANGENT]   = (GLsizei)(_dataBiTangent.size() * sizeof(vec3<GLfloat>));
    nSize[ATTRIB_BONE_WEIGHT] = (GLsizei)(_boneWeights.size()   * sizeof(vec4<GLfloat>));
    nSize[ATTRIB_BONE_INDICE] = (GLsizei)(_boneIndices.size()   * sizeof(vec4<GLubyte>));
    GLsizei nSizeIndices      = (GLsizei)(_largeIndices ? _hardwareIndicesL.size() * sizeof(GLuint) : 
                                                          _hardwareIndicesS.size() * sizeof(GLushort));
    // If any of the VBO's components changed size, we need to recreate the entire buffer.
    bool sizeChanged = false;
    for (U8 i = 0; i < VertexAttribute_PLACEHOLDER; ++i) {
        if (nSize[i] != _prevSize[i]) {
            sizeChanged = true;
            _prevSize[i] = nSize[i];
        }
    }
    // Same logic applies to indices.
    bool indicesChanged = (nSizeIndices != _prevSizeIndices);
    _prevSizeIndices = nSizeIndices;
    _refreshQueued = sizeChanged || indicesChanged;

    if (!_refreshQueued) {
        for (U8 i = 0; i < VertexAttribute_PLACEHOLDER; ++i) {
            if (_attribDirty[i]) {
                _refreshQueued = true;
                break;
            }
        }
    }

    if (!_refreshQueued) {
        return false;
    }

    // Bind the current VAO to save our attributes
    GL_API::setActiveVAO(_VAOid);
    // Bind the the vertex buffer and index buffer
    GL_API::setActiveBuffer(GL_ARRAY_BUFFER, _VBid);
    GL_API::setActiveBuffer(GL_ELEMENT_ARRAY_BUFFER, _IBid);
    // Check if we have any animation data.
    _animationData = (!_boneWeights.empty() && !_boneIndices.empty());
    // Refresh buffer data (if this is the first call to refresh, this will be true)
    if (sizeChanged) {
        // Calculated the needed size for our buffer
        GLsizei vbSize = 0;
        for (U8 i = 0; i < VertexAttribute_PLACEHOLDER; ++i) {
            vbSize += nSize[i];
        }
        Console::d_printfn(Locale::get("DISPLAY_VB_METRICS"), vbSize, 4 * 1024 * 1024);
        // Allocate sufficient space in our buffer
        glBufferData(GL_ARRAY_BUFFER, vbSize, NULL, _usage);
        // Refresh all attributes
        memset(_attribDirty, true, VertexAttribute_PLACEHOLDER * sizeof(bool));
    }

    // Get the buffer data for each attribute
    const void* dataBuffer[VertexAttribute_PLACEHOLDER];
    dataBuffer[ATTRIB_POSITION]    = _dataPosition.empty()  ? 0 : _dataPosition.data();
    dataBuffer[ATTRIB_COLOR]       = _dataColor.empty()     ? 0 : _dataColor.data();
    dataBuffer[ATTRIB_NORMAL]      = _dataNormal.empty()    ? 0 : _dataNormal.data();
    dataBuffer[ATTRIB_TEXCOORD]    = _dataTexcoord.empty()  ? 0 : _dataTexcoord.data();
    dataBuffer[ATTRIB_TANGENT]     = _dataTangent.empty()   ? 0 : _dataTangent.data();
    dataBuffer[ATTRIB_BITANGENT]   = _dataBiTangent.empty() ? 0 : _dataBiTangent.data();
    dataBuffer[ATTRIB_BONE_WEIGHT] = _boneWeights.empty()   ? 0 : _boneWeights.data();
    dataBuffer[ATTRIB_BONE_INDICE] = _boneIndices.empty()   ? 0 : _boneIndices.data();

    // Get position data offset
    I32 attrib = ATTRIB_POSITION;
    _VBoffset[attrib] = 0;
    GLsizei previousOffset = _VBoffset[attrib];
    GLsizei previousCount = nSize[attrib];
    // Loop over all possible attributes
    for (; attrib < VertexAttribute_PLACEHOLDER; ++attrib) {
        // Only process available attributes
        if (nSize[attrib] > 0) {
            // Get it's offset
            if (attrib > ATTRIB_POSITION) {
                _VBoffset[attrib] = previousOffset + previousCount;
                previousOffset = _VBoffset[attrib];
                previousCount = nSize[attrib];
            }
            // Add it to the VBO
            if (_attribDirty[attrib]) {
                glBufferSubData(GL_ARRAY_BUFFER, _VBoffset[attrib], nSize[attrib], dataBuffer[attrib]);
                // Clear the attribute's dirty flag
                _attribDirty[attrib] = false;
            }
        }
    }
    // Check if we need to update the IBO (will be true for the first Refresh() call)
    if (indicesChanged) {
        //Update our IB
        if (_largeIndices) {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
                         _hardwareIndicesL.size() * sizeof(GLuint),
                         _hardwareIndicesL.data(), 
                         GL_STATIC_DRAW);
        } else {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         _hardwareIndicesS.size() * sizeof(GLushort),
                         _hardwareIndicesS.data(),
                         GL_STATIC_DRAW);
        }
    }
    // Set vertex attribute pointers    
    Upload_VB_Attributes();
    // Validate the buffer
    checkStatus();
    // Possibly clear client-side buffer for all non-required attributes?
    // foreach attribute if !required then delete else skip ?
    _refreshQueued = false;
    return true;
}

/// This method creates the initial VAO and VB OpenGL objects and queues a Refresh call
bool glVertexArray::CreateInternal() {
    // Avoid double create calls
    DIVIDE_ASSERT(!_created, "glVertexArray error: Attempted to double create a VB");
    _created = false;
    // Position data is a minim requirement
    if (_dataPosition.empty()) {
        Console::errorfn(Locale::get("ERROR_VB_POSITION"));
        return _created;
    }
    
    _formatInternal = GLUtil::GL_ENUM_TABLE::glDataFormat[_format];
    // Enforce all update flags. Kind of useless, but it doesn't hurt
    memset(_attribDirty, true, VertexAttribute_PLACEHOLDER * sizeof(bool));
    // Generate a "Vertex Array Object"
    glGenVertexArrays(1, &_VAOid);
    // Generate a new Vertex Buffer Object
    glGenBuffers(1, &_VBid);
    // Generate an "Index Buffer Object"
    glGenBuffers(1, &_IBid);
    // Validate buffer creation
    DIVIDE_ASSERT(_VAOid != 0, Locale::get("ERROR_VAO_INIT"));
    DIVIDE_ASSERT(_VBid != 0, Locale::get("ERROR_VB_INIT"));
    // Assert if the IB creation failed
    DIVIDE_ASSERT(_IBid != 0, Locale::get("ERROR_IB_INIT"));
    // Calling refresh updates all stored information and sends it to the GPU
    _created = queueRefresh();
    return _created;
}

/// Render the current buffer data using the specified command
void glVertexArray::Draw(const GenericDrawCommand& command, bool skipBind) {
    U32 instanceCount = command.cmd().instanceCount;
    DIVIDE_ASSERT(command.primitiveType() != PrimitiveType_PLACEHOLDER, "glVertexArray error: Draw command's type is not valid!");
    // Instance count can be generated programmatically, so we need to make sure it's valid
    if (instanceCount == 0) {
        return;
    }
    // There are cases when re-binding the buffer isn't needed (e.g. tight loops)
    if (!skipBind) {
        // Make sure the buffer is current
        if (!SetActive()) {
            return;
        }
    }
    // Process the actual draw command
    if (!Config::Profile::DISABLE_DRAWS) {
        // Submit the draw command
        glDrawElementsIndirect(command.renderWireframe() ? GL_LINE_LOOP : 
                                                           GLUtil::GL_ENUM_TABLE::glPrimitiveTypeTable[command.primitiveType()],
                               _formatInternal,
                               (void*)(command.cmd().baseInstance * sizeof(IndirectDrawCommand)));
    }
    // Always update draw call counter after draw calls
    GFX_DEVICE.registerDrawCall();
}

/// Submit multiple draw commands using the current buffer
void glVertexArray::Draw(const vectorImpl<GenericDrawCommand>& commands, bool skipBind) {
    // This should be reimplemented with indirect draw buffers!
    DIVIDE_ASSERT(!commands.empty(), "glVertexArray error: empty list of draw commands received for the current draw call");
    // Set the current buffer as active
    if (!skipBind) {
        if (!SetActive()) {
            return;
        }
    }
    // Send each command in turn to the GPU, but don't try to rebind the buffer every time
    for(const GenericDrawCommand& cmd : commands) {
        Draw(cmd, true);
    }
}

/// Set the current buffer as active
bool glVertexArray::SetActive(){
    // Make sure we have valid data (buffer creation is deferred to the first activate call)
    if (!_created) {
        if (!CreateInternal()) {
            return false;
        }
    }
    // Check if we have a refresh request queued up
    if (_refreshQueued) {
        if (!Refresh()) {
            return false;
        }
    }

    // Bind the vertex array object that in turn activates all of the bindings and pointers set on creation
    if (GL_API::setActiveVAO(_VAOid)) {
        // If this is the first time the VAO is bound in the current loop, check for primitive restart requests
        GL_API::togglePrimitiveRestart(_primitiveRestartEnabled, !_largeIndices);
    }
    return true;
}

/// Activate and set all of the required vertex attributes.
void glVertexArray::Upload_VB_Attributes(){
    glEnableVertexAttribArray(VERTEX_POSITION_LOCATION);
    glVertexAttribPointer(VERTEX_POSITION_LOCATION, 3,
                                  GL_FLOAT, GL_FALSE, sizeof(vec3<GLfloat>),
                                  (void*)(_VBoffset[ATTRIB_POSITION]));

    if (!_dataColor.empty()) {
        glEnableVertexAttribArray(VERTEX_COLOR_LOCATION);
        glVertexAttribPointer(VERTEX_COLOR_LOCATION, 3, 
                                      GL_UNSIGNED_BYTE, GL_FALSE, sizeof(vec3<GLubyte>),
                                      (void*)(_VBoffset[ATTRIB_COLOR]));
    } else {
        glDisableVertexAttribArray(VERTEX_COLOR_LOCATION);
    }

    if (!_dataNormal.empty()) {
        glEnableVertexAttribArray(VERTEX_NORMAL_LOCATION);
        glVertexAttribPointer(VERTEX_NORMAL_LOCATION, 3,
                                      GL_FLOAT, GL_FALSE, sizeof(vec3<GLfloat>),
                                      (void*)(_VBoffset[ATTRIB_NORMAL]));
    } else {
        glDisableVertexAttribArray(VERTEX_NORMAL_LOCATION);
    }

    if (!_dataTexcoord.empty()) {
        glEnableVertexAttribArray(VERTEX_TEXCOORD_LOCATION);
        glVertexAttribPointer(VERTEX_TEXCOORD_LOCATION, 2,
                                      GL_FLOAT, GL_FALSE, sizeof(vec2<GLfloat>),
                                      (void*)(_VBoffset[ATTRIB_TEXCOORD]));
    } else {
        glDisableVertexAttribArray(VERTEX_TEXCOORD_LOCATION);
    }

    if (!_dataTangent.empty()) {
        glEnableVertexAttribArray(VERTEX_TANGENT_LOCATION);
        glVertexAttribPointer(VERTEX_TANGENT_LOCATION, 3,
                                      GL_FLOAT, GL_FALSE, sizeof(vec3<GLfloat>),
                                      (void*)(_VBoffset[ATTRIB_TANGENT]));
    } else {
        glDisableVertexAttribArray(VERTEX_TANGENT_LOCATION);
    }

    if (!_dataBiTangent.empty()) {
        glEnableVertexAttribArray(VERTEX_BITANGENT_LOCATION);
        glVertexAttribPointer(VERTEX_BITANGENT_LOCATION, 3,
                                      GL_FLOAT, GL_FALSE, sizeof(vec3<GLfloat>),
                                      (void*)(_VBoffset[ATTRIB_BITANGENT]));
    } else {
        glDisableVertexAttribArray(VERTEX_BITANGENT_LOCATION);
    }

    if (_animationData) {
        glEnableVertexAttribArray(VERTEX_BONE_WEIGHT_LOCATION);
        glVertexAttribPointer(VERTEX_BONE_WEIGHT_LOCATION, 4,
                                      GL_FLOAT, GL_FALSE, sizeof(vec4<GLfloat>),
                                      (void*)(_VBoffset[ATTRIB_BONE_WEIGHT]));

        glEnableVertexAttribArray(VERTEX_BONE_INDICE_LOCATION);
        glVertexAttribIPointer(VERTEX_BONE_INDICE_LOCATION, 4,
                                       GL_UNSIGNED_BYTE, sizeof(vec4<GLubyte>),
                                       (void*)(_VBoffset[ATTRIB_BONE_INDICE]));
    } else {
        glDisableVertexAttribArray(VERTEX_BONE_WEIGHT_LOCATION);
        glDisableVertexAttribArray(VERTEX_BONE_INDICE_LOCATION);
    }
}

/// Various post-create checks
void glVertexArray::checkStatus(){
}

};