#include "Headers/glVertexArray.h"
#include "Hardware/Video/Headers/GFXDevice.h"
#include "Managers/Headers/ShaderManager.h"

#define VOID_PTR_CAST(X) (const GLvoid*)(X)

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
    _VAOid = _VBid = _IBid = _indirectDrawBuffer = 0;
    _prevLoD = 250;
    for(U8 i = 0; i < Config::SCENE_NODE_LOD; ++i)
        _lodBatches[i].reserve(MAX_DRAW_COMMANDS);
    _multiCount.reserve(MAX_DRAW_COMMANDS);
    _multiIndices.reserve(MAX_DRAW_COMMANDS);
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
    if(_indirectDrawBuffer > 0) glDeleteBuffers(1, &_indirectDrawBuffer);
    _VAOid = _VBid = _IBid = _indirectDrawBuffer = 0;
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
    //Dynamic LOD elements (such as terrain) need dynamic indices
    //We can manually override indice usage (again, used by the Terrain rendering system)
    DIVIDE_ASSERT(!_hardwareIndicesL.empty() || !_hardwareIndicesS.empty(), "glVertexArray error: Invalid indice data on Refresh()!");

    GLsizei nSize[VertexAttribute_PLACEHOLDER];
    //Get the size of each data container
    nSize[ATTRIB_POSITION]    = (GLsizei)(_dataPosition.size()  * sizeof(vec3<GLfloat>));
    nSize[ATTRIB_COLOR]       = (GLsizei)(_dataColor.size()     * sizeof(vec3<GLubyte>));
    nSize[ATTRIB_NORMAL]      = (GLsizei)(_dataNormal.size()    * sizeof(vec3<GLfloat>));
    nSize[ATTRIB_TEXCOORD]    = (GLsizei)(_dataTexcoord.size()  * sizeof(vec2<GLfloat>));
    nSize[ATTRIB_TANGENT]     = (GLsizei)(_dataTangent.size()   * sizeof(vec3<GLfloat>));
    nSize[ATTRIB_BITANGENT]   = (GLsizei)(_dataBiTangent.size() * sizeof(vec3<GLfloat>));
    nSize[ATTRIB_BONE_WEIGHT] = (GLsizei)(_boneWeights.size()   * sizeof(vec4<GLfloat>));
    nSize[ATTRIB_BONE_INDICE] = (GLsizei)(_boneIndices.size()   * sizeof(vec4<GLubyte>));
    GLsizei nSizeIndices      = (GLsizei)(_largeIndices ? _hardwareIndicesL.size() * sizeof(GLuint) : _hardwareIndicesS.size() * sizeof(GLushort));
    // if any of the VBO's components changed size, we need to recreate the entire buffer.
    // could be optimized, I know, but ... 
    bool sizeChanged = false;
    
    for (U8 i = 0; i < VertexAttribute_PLACEHOLDER; ++i){
        if (nSize[i] != _prevSize[i]){
            sizeChanged = true;
            _prevSize[i] = nSize[i];
        }
    }

    bool indicesChanged = (nSizeIndices != _prevSizeIndices);

    _prevSizeIndices = nSizeIndices;
   
    _refreshQueued = sizeChanged || indicesChanged;

    if (!_refreshQueued)
        for (U8 i = 0; i < VertexAttribute_PLACEHOLDER; ++i)
            if (_attribDirty[i]){
                _refreshQueued = true;
                break;
            }
  
    if (!_refreshQueued)
        return true;

    //Bind the current VAO to save our attribs
    GL_API::setActiveVAO(_VAOid);
    //Bind the the vertex buffer and index buffer
    GL_API::setActiveBuffer(GL_ARRAY_BUFFER, _VBid);
    GL_API::setActiveBuffer(GL_ELEMENT_ARRAY_BUFFER, _IBid);

    //Check if we have any animation data.
    _animationData = (!_boneWeights.empty() && !_boneIndices.empty());

    if (sizeChanged){
        //get the 8-bit size factor of the VB
        GLsizei vbSize = 0;
        for (U8 i = 0; i < VertexAttribute_PLACEHOLDER; ++i)
            vbSize += nSize[i];

        D_PRINT_FN(Locale::get("DISPLAY_VB_METRICS"), vbSize, 4 * 1024 * 1024);
        //Create our regular VB with all of the data packed in it
        glBufferData(GL_ARRAY_BUFFER, vbSize, NULL, _usage);
        memset(_attribDirty, true, VertexAttribute_PLACEHOLDER * sizeof(bool));
    }

    const GLvoid* offsets[VertexAttribute_PLACEHOLDER];
    offsets[ATTRIB_POSITION]    = _dataPosition.empty()  ? 0 : VOID_PTR_CAST(&_dataPosition.front().x);
    offsets[ATTRIB_COLOR]       = _dataColor.empty()     ? 0 : VOID_PTR_CAST(&_dataColor.front().r);
    offsets[ATTRIB_NORMAL]      = _dataNormal.empty()    ? 0 : VOID_PTR_CAST(&_dataNormal.front().x);
    offsets[ATTRIB_TEXCOORD]    = _dataTexcoord.empty()  ? 0 : VOID_PTR_CAST(&_dataTexcoord.front().s);
    offsets[ATTRIB_TANGENT]     = _dataTangent.empty()   ? 0 : VOID_PTR_CAST(&_dataTangent.front().x);
    offsets[ATTRIB_BITANGENT]   = _dataBiTangent.empty() ? 0 : VOID_PTR_CAST(&_dataBiTangent.front().x);
    offsets[ATTRIB_BONE_WEIGHT] = _boneWeights.empty()   ? 0 : VOID_PTR_CAST(&_boneWeights.front().x);
    offsets[ATTRIB_BONE_INDICE] = _boneIndices.empty()   ? 0 : VOID_PTR_CAST(&_boneIndices.front().x);

    //Get Position offset
    I32 attrib = ATTRIB_POSITION;
    _VBoffset[attrib] = 0;
    GLsizei previousOffset = _VBoffset[attrib];
    GLsizei previousCount = nSize[attrib];
    
    for (; attrib < VertexAttribute_PLACEHOLDER; ++attrib){
        if (nSize[attrib] > 0) {
            if (attrib > ATTRIB_POSITION){
                _VBoffset[attrib] = previousOffset + previousCount;
                previousOffset = _VBoffset[attrib];
                previousCount = nSize[attrib];
            }
            if(_attribDirty[attrib]){
                glBufferSubData(GL_ARRAY_BUFFER, _VBoffset[attrib], nSize[attrib], offsets[attrib]);
                _attribDirty[attrib] = false;
            }
        }
    }
   
    if (indicesChanged){
        //Update our IB
        if(_largeIndices) glBufferData(GL_ELEMENT_ARRAY_BUFFER, _hardwareIndicesL.size() * sizeof(GLuint),   VOID_PTR_CAST(&_hardwareIndicesL.front()), GL_STATIC_DRAW);
        else              glBufferData(GL_ELEMENT_ARRAY_BUFFER, _hardwareIndicesS.size() * sizeof(GLushort), VOID_PTR_CAST(&_hardwareIndicesS.front()), GL_STATIC_DRAW);
    }

    
    Upload_VB_Attributes();
    
    checkStatus();

    _refreshQueued = false;
    return true;
}

/// This method creates the initial VB
/// The only difference between Create and Refresh is the generation of a new buffer
bool glVertexArray::CreateInternal() {
    DIVIDE_ASSERT(!_created, "glVertexArray error: Attempted to double create a VB");

    if(_dataPosition.empty()){
        ERROR_FN(Locale::get("ERROR_VB_POSITION"));
        _created = false;
        return _created;
    }
    _formatInternal = glDataFormat[_format];
    // Enforce all update flags. Kind of useless, but it doesn't hurt
    memset(_attribDirty, true, VertexAttribute_PLACEHOLDER * sizeof(bool));
    // Generate a "Vertex Array Object"
    glGenVertexArrays(1, &_VAOid);
    // Generate a new Vertex Buffer Object
    glGenBuffers(1, &_VBid);
    // Generate an "Index Buffer Object"
    glGenBuffers(1, &_IBid);
    // Generate an Indirect Draw Buffer
    glGenBuffers(1, &_indirectDrawBuffer);
    // Validate buffer creation
    DIVIDE_ASSERT(_VAOid != 0, Locale::get("ERROR_VAO_INIT"));
    DIVIDE_ASSERT(_VBid != 0, Locale::get("ERROR_VB_INIT"));
    //Show an error and assert if the IB creation failed
    DIVIDE_ASSERT(_IBid != 0, Locale::get("ERROR_IB_INIT"));

    // calling refresh updates all stored information and sends it to the GPU
    _created = queueRefresh();
    return _created;
}

void glVertexArray::Draw(const GenericDrawCommand& command, bool skipBind) {
    U32 instanceCount = command._cmd.instanceCount;
    assert(command._type != PrimitiveType_PLACEHOLDER);

    if(instanceCount == 0)
        return;

    if(!skipBind)
        if(!SetActive())
            return;
    
    if(_prevLoD != command._lodIndex){
        _prevLoD = command._lodIndex;
        _currentShader->SetLOD(_prevLoD);
    }

    if(!Config::Profile::DISABLE_DRAWS){
        IndirectDrawCommand cmd = command._cmd;
        
        if(cmd.count == 0) cmd.count = (GLsizei)(_largeIndices ? _hardwareIndicesL.size() : _hardwareIndicesS.size());
        //GL_API::setActiveBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectDrawBuffer);
        //glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(IndirectDrawCommand), &cmd, GL_DYNAMIC_COPY);
        //glDrawElementsIndirect(glPrimitiveTypeTable[command._type], _formatInternal, //BUFFER_OFFSET(0));
        //GL_API::setActiveBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
        const GLvoid* bufferOffset = BUFFER_OFFSET(cmd.firstIndex * (_largeIndices ? sizeof(GLuint) : sizeof(GLushort)));
        glDrawElements(glPrimitiveTypeTable[command._type], cmd.count, _formatInternal, bufferOffset);

    }

    GL_API::registerDrawCall();
}

void glVertexArray::Draw(const vectorImpl<GenericDrawCommand>& commands, bool skipBind) {
    assert(!commands.empty());

    //SetActive returns immediately if it's already been called
    if(!skipBind)
        if(!SetActive())
            return;

    if(commands.size() == 1){
        Draw(commands[0], true);
        return;
    }else if(!Config::BATCH_DRAW_COMMANDS || commands[0]._cmd.instanceCount > 1){
        for(const GenericDrawCommand& cmd : commands)
            Draw(cmd, true);
        return;
    }

    DIVIDE_ASSERT(commands.size() < MAX_DRAW_COMMANDS, "glVertexArray error: too many draw commands received!");

    for(U8 i = 0; i < Config::SCENE_NODE_LOD; ++i)
        _lodBatches[i].resize(0);

    for (U8 i = 0; i < commands.size(); ++i)
        _lodBatches[commands[i]._lodIndex].push_back(i);

    GLsizei size = (_largeIndices ? sizeof(GLuint) : sizeof(GLushort));
    for(U8 i = 0; i < Config::SCENE_NODE_LOD; ++i){
        if(_lodBatches[i].empty()) continue;

        _multiCount.resize(0);
        _multiIndices.resize(0);
        if(_prevLoD != i){
            _prevLoD = i;
            _currentShader->SetLOD(_prevLoD);
        }

        for (U32 commandIndex : _lodBatches[i]){
            const GenericDrawCommand& cmd = commands[commandIndex];
            _multiCount.push_back(cmd._cmd.count);
            _multiIndices.push_back(BUFFER_OFFSET(cmd._cmd.firstIndex * size));
        }

        if(!Config::Profile::DISABLE_DRAWS)
            glMultiDrawElements(glPrimitiveTypeTable[commands[0]._type], &_multiCount.front(), _formatInternal, &_multiIndices.front(), (GLsizei)_lodBatches[i].size());

        GL_API::registerDrawCall();
    }
}

/// If we do not have a VAO, we use vertex arrays as fail safes
bool glVertexArray::SetActive(){
    //If we do not use a shader, than most states are texCoord based, so a VAO is useless
    DIVIDE_ASSERT(_currentShader != nullptr && _currentShader->getId() != 0, Locale::get("ERROR_VAO_SHADER"));

    //if the shader is not ready, do not draw anything
    if(!_currentShader->isBound() || GFXDevice::getActiveProgram()->getId() != _currentShader->getId())
        return false;

    // Make sure we have valid data
    if (!_created) CreateInternal();

    // Make sure we do not have a refresh request queued up
    if(_refreshQueued)
        if(!Refresh())
            return false;

    //Choose optimal VAO/VB combo
    if (GL_API::setActiveVAO(_VAOid))
        GL_API::togglePrimitiveRestart(_primitiveRestartEnabled, !_largeIndices);

    _prevLoD = 250;
    //Send our transformation matrices (projection, world, view, inv model view, etc)
    currentShader()->uploadNodeMatrices();

    return true;
}

//Bind vertex attributes 
void glVertexArray::Upload_VB_Attributes(){
    glEnableVertexAttribArray(Divide::VERTEX_POSITION_LOCATION);
    glVertexAttribPointer(Divide::VERTEX_POSITION_LOCATION, 3,
                                  GL_FLOAT, GL_FALSE, sizeof(vec3<GLfloat>),
                                  BUFFER_OFFSET(_VBoffset[ATTRIB_POSITION]));

    if (!_dataColor.empty()) {
        glEnableVertexAttribArray(Divide::VERTEX_COLOR_LOCATION);
        glVertexAttribPointer(Divide::VERTEX_COLOR_LOCATION, 3, 
                                      GL_UNSIGNED_BYTE, GL_FALSE, sizeof(vec3<GLubyte>),
                                      BUFFER_OFFSET(_VBoffset[ATTRIB_COLOR]));
    }else{
        glDisableVertexAttribArray(Divide::VERTEX_COLOR_LOCATION);
    }

    if (!_dataNormal.empty()) {
        glEnableVertexAttribArray(Divide::VERTEX_NORMAL_LOCATION);
        glVertexAttribPointer(Divide::VERTEX_NORMAL_LOCATION, 3,
                                      GL_FLOAT, GL_FALSE, sizeof(vec3<GLfloat>),
                                      BUFFER_OFFSET(_VBoffset[ATTRIB_NORMAL]));
    }else{
        glDisableVertexAttribArray(Divide::VERTEX_NORMAL_LOCATION);
    }

    if(!_dataTexcoord.empty()) {
        glEnableVertexAttribArray(Divide::VERTEX_TEXCOORD_LOCATION);
        glVertexAttribPointer(Divide::VERTEX_TEXCOORD_LOCATION, 2,
                                      GL_FLOAT, GL_FALSE, sizeof(vec2<GLfloat>),
                                      BUFFER_OFFSET(_VBoffset[ATTRIB_TEXCOORD]));
    }else{
        glDisableVertexAttribArray(Divide::VERTEX_TEXCOORD_LOCATION);
    }

    if (!_dataTangent.empty()){
        glEnableVertexAttribArray(Divide::VERTEX_TANGENT_LOCATION);
        glVertexAttribPointer(Divide::VERTEX_TANGENT_LOCATION, 3,
                                      GL_FLOAT, GL_FALSE, sizeof(vec3<GLfloat>),
                                      BUFFER_OFFSET(_VBoffset[ATTRIB_TANGENT]));
    }else{
        glDisableVertexAttribArray(Divide::VERTEX_TANGENT_LOCATION);
    }

    if(!_dataBiTangent.empty()){
        glEnableVertexAttribArray(Divide::VERTEX_BITANGENT_LOCATION);
        glVertexAttribPointer(Divide::VERTEX_BITANGENT_LOCATION, 3,
                                      GL_FLOAT, GL_FALSE, sizeof(vec3<GLfloat>),
                                      BUFFER_OFFSET(_VBoffset[ATTRIB_BITANGENT]));
    }else{
        glDisableVertexAttribArray(Divide::VERTEX_BITANGENT_LOCATION);
    }

    if(_animationData){
        //Bone weights
        glEnableVertexAttribArray(Divide::VERTEX_BONE_WEIGHT_LOCATION);
        glVertexAttribPointer(Divide::VERTEX_BONE_WEIGHT_LOCATION, 4,
                                      GL_FLOAT, GL_FALSE, sizeof(vec4<GLfloat>),
                                      BUFFER_OFFSET(_VBoffset[ATTRIB_BONE_WEIGHT]));

        // Bone indices
        glEnableVertexAttribArray(Divide::VERTEX_BONE_INDICE_LOCATION);
        glVertexAttribIPointer(Divide::VERTEX_BONE_INDICE_LOCATION, 4,
                                       GL_UNSIGNED_BYTE, sizeof(vec4<GLubyte>),
                                       BUFFER_OFFSET(_VBoffset[ATTRIB_BONE_INDICE]));
    }else{
        glDisableVertexAttribArray(Divide::VERTEX_BONE_WEIGHT_LOCATION);
        glDisableVertexAttribArray(Divide::VERTEX_BONE_INDICE_LOCATION);
    }
}

//Various post-create checks
void glVertexArray::checkStatus(){
}