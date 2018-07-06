#include "Hardware/Video/OpenGL/Headers/glResources.h"
#include "core.h"
#include "Headers/glVertexArrayObject.h"
#include "Hardware/Video/Headers/GFXDevice.h"

/// Default destructor
glVertexArrayObject::glVertexArrayObject() : VertexBufferObject() {
	_useVA = false;
	_created = false;
	_animationData = false;
	_refreshQueued = false;
	_VAOid = 0;
}

/// Delete buffer
void glVertexArrayObject::Destroy(){
	if(_VBOid > 0){
		GLCheck(glDeleteBuffers(1, &_VBOid));
	}
	if(_IBOid > 0){
		GLCheck(glDeleteBuffers(1, &_IBOid));
	}
	if(_VAOid > 0){
		GLCheck(glDeleteVertexArrays(1, &_VAOid));
	}
}

/// Create a dynamic or static VBO
bool glVertexArrayObject::Create(bool staticDraw){	
	_staticDraw = staticDraw;
	return true;
}

bool glVertexArrayObject::queueRefresh() {
	if(_created) return Refresh();
	else _refreshQueued = true;
	return true;	
}

/// Update internal data based on construction method (dynamic/static)
bool glVertexArrayObject::Refresh(){
	///Skip for vertex arrays
	if(_useVA) {
		_refreshQueued = false;
		return true;
	}

	//Dynamic LOD elements (such as terrain) need dynamic indices
	bool skipIndiceBuffer = _hardwareIndices.empty() ? true : false;

	assert(!_dataPosition.empty());
	GLuint usage = GL_STATIC_DRAW;

	if(!_staticDraw){
		(GFX_DEVICE.getApi() == OpenGLES) ?  usage = GL_STREAM_DRAW :  usage = GL_DYNAMIC_DRAW;
	}

	ptrdiff_t nSizePosition    = _dataPosition.size()*sizeof(vec3<GLfloat>);
	ptrdiff_t nSizeNormal      = _dataNormal.size()*sizeof(vec3<GLfloat>);
	ptrdiff_t nSizeTexcoord    = _dataTexcoord.size()*sizeof(vec2<GLfloat>);
	ptrdiff_t nSizeTangent     = _dataTangent.size()*sizeof(vec3<GLfloat>);
	ptrdiff_t nSizeBiTangent   = _dataBiTangent.size()*sizeof(vec3<GLfloat>);
	ptrdiff_t nSizeBoneWeights = _boneWeights.size()*sizeof(vec4<GLfloat>);
	ptrdiff_t nSizeBoneIndices = _boneIndices.size()*sizeof(vec4<GLushort>);
	
	_animationData = (!_boneWeights.empty() && !_boneIndices.empty());

	_VBOoffsetPosition  = 0;
	ptrdiff_t previousOffset = _VBOoffsetPosition;
	ptrdiff_t previousCount = nSizePosition;

	if(nSizeNormal > 0){
		_VBOoffsetNormal = previousOffset + previousCount;
		previousOffset = _VBOoffsetNormal;
		previousCount = nSizeNormal;
	}

	if(nSizeTexcoord > 0){
		_VBOoffsetTexcoord	= previousOffset + previousCount;
		previousOffset = _VBOoffsetTexcoord;
		previousCount = nSizeTexcoord;
	}

	if(nSizeTangent > 0){
		_VBOoffsetTangent	= previousOffset + previousCount;
		previousOffset = _VBOoffsetTangent;
		previousCount = nSizeTangent;
	}

	if(nSizeBiTangent > 0){
		_VBOoffsetBiTangent	  = previousOffset + previousCount;
		previousOffset = _VBOoffsetBiTangent;
		previousCount = nSizeBiTangent;
	}

	if(_animationData){
		/// Bone indices and weights are always packed togheter to prevent invalid animations
		_VBOoffsetBoneWeights = previousOffset + previousCount;
		previousOffset = _VBOoffsetBoneWeights;
		previousCount   = nSizeBoneWeights;
		_VBOoffsetBoneIndices = previousOffset + previousCount;
	}

	GLCheck(glBindVertexArray(_VAOid));
	if(!skipIndiceBuffer){
		GLCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _IBOid));
		GLCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, _hardwareIndices.size() * sizeof(GLushort),
													  (const GLvoid*)(&_hardwareIndices[0]), 
													  usage));
	}
	
	GLCheck(glBindBuffer(GL_ARRAY_BUFFER, _VBOid));
	GLCheck(glBufferData(GL_ARRAY_BUFFER, nSizePosition+
									      nSizeNormal+
										  nSizeTexcoord+
										  nSizeTangent+
										  nSizeBiTangent+
										  nSizeBoneWeights+
										  nSizeBoneIndices,
										  0, usage));

	if(_positionDirty){
	    GLCheck(glBufferSubData(GL_ARRAY_BUFFER, _VBOoffsetPosition,
												 nSizePosition,	    
												 (const GLvoid*)(&_dataPosition[0].x)));
		/// Clear all update flags
		_positionDirty  = false;
	}

	if(_dataNormal.size() && _normalDirty){
		for(U32 i = 0; i < _dataNormal.size(); i++){
			//g_normalsSmall.push_back(UP1010102(_dataNormal[i].x,_dataNormal[i].z,_dataNormal[i].y,1.0f));
		}
		GLCheck(glBufferSubData(GL_ARRAY_BUFFER, _VBOoffsetNormal,
												 nSizeNormal,	   
											     (const GLvoid*)(&_dataNormal[0].x)));
		_normalDirty    = false;
	}

	if(_dataTexcoord.size() && _texcoordDirty){
		GLCheck(glBufferSubData(GL_ARRAY_BUFFER, _VBOoffsetTexcoord,
												 nSizeTexcoord,   	
												 (const GLvoid*)(&_dataTexcoord[0].s)));
		_texcoordDirty  = false;
	}

	if(_dataTangent.size() && _tangentDirty){
		GLCheck(glBufferSubData(GL_ARRAY_BUFFER, _VBOoffsetTangent,
											     nSizeTangent,	    
												 (const GLvoid*)(&_dataTangent[0].x)));
		_tangentDirty   = false;
	}

	if(_dataBiTangent.size() && _bitangentDirty){
		GLCheck(glBufferSubData(GL_ARRAY_BUFFER, _VBOoffsetBiTangent,
											     nSizeBiTangent,	    
												 (const GLvoid*)(&_dataBiTangent[0].x)));
		_bitangentDirty = false;
	}

	if(_animationData) {
		if(_indicesDirty){
			GLCheck(glBufferSubData(GL_ARRAY_BUFFER, _VBOoffsetBoneIndices,
												     nSizeBoneIndices,	
													 (const GLvoid*)(&_boneIndices[0].x)));
			_indicesDirty   = false;
		}

		if(_weightsDirty){
			GLCheck(glBufferSubData(GL_ARRAY_BUFFER, _VBOoffsetBoneWeights,
													 nSizeBoneWeights,	
													 (const GLvoid*)(&_boneWeights[0].x)));
			_weightsDirty   = false;
		}

	}

	/// If we do not have a shader assigned, use old style VBO
	if(_currentShader == NULL){
		Enable_VBO();		
	}else{ /// Else, use attrib pointers
		Enable_Shader_VBO();
	}
	GLCheck(glBindVertexArray(0));
	checkStatus();

	

	_refreshQueued = false;
	return true;
}


/// This method creates the initial VBO
/// The only diferrence between Create and Refresh is the generation of a new buffer
bool glVertexArrayObject::CreateInternal() {
	/// Enforce all update flags. Kinda useless, but it doesn't hurt
	_positionDirty  = true;
	_normalDirty    = true;
	_texcoordDirty  = true;
	_tangentDirty   = true;
	_bitangentDirty = true;
	_indicesDirty   = true;
	_weightsDirty   = true;

	/// Generate a "VertexArrayObject"
	GLCheck(glGenVertexArrays(1, &_VAOid));
	/// Generate a new buffer
	GLCheck(glGenBuffers(1, &_VBOid));
	/// Generate an "IndexBufferObject"
	GLCheck(glGenBuffers(1, &_IBOid));
	if(_dataPosition.empty()){
		ERROR_FN(Locale::get("ERROR_VBO_POSITION"));
		return false;
	}
	/// Validate buffer creation
	if(!_VBOid || !_IBOid || !_VAOid) {
		ERROR_FN(Locale::get("ERROR_VBO_INIT"));
		_useVA = true;
	}else {
		/// calling refresh updates all stored information and sends it to the GPU
		_created = queueRefresh();
	}
	_created = true;
	return _created;
}

/// Inform the VBO what shader we use to render the object so that the data is uploaded correctly
void glVertexArrayObject::setShaderProgram(ShaderProgram* const shaderProgram) {
	if(shaderProgram->getName().compare("NULL_SHADER") == 0){
		_currentShader = NULL;
		queueRefresh();
	}else{
		_currentShader = shaderProgram;
	}
}

///This draw method needs an Index Buffer!
void glVertexArrayObject::Draw(PrimitiveType type, U8 LODindex){
	CLAMP<U8>(LODindex,0,_indiceLimits.size() - 1);
	
	assert(_indiceLimits[LODindex].y > _indiceLimits[LODindex].x);
	GLsizei count = _hardwareIndices.size();
	assert(_indiceLimits[LODindex].y < count);

	Enable();


	GLCheck(glDrawRangeElements(glPrimitiveTypeTable[type], 
		                        _indiceLimits[LODindex].x,
								_indiceLimits[LODindex].y,
								count,
								GL_UNSIGNED_SHORT,
								BUFFER_OFFSET(0)));

	Disable();
}

/// If we do not have a VAO object, we use vertex arrays as fail safes
void glVertexArrayObject::Enable(){
	if(!_created) CreateInternal(); ///< Make sure we have valid data
	if(_refreshQueued) Refresh(); ///< Make sure we do not have a refresh request queued up

	if(!_useVA){
		GLCheck(glBindVertexArray(_VAOid));
	}else{
		Enable_VA();		
	}
}
/// If we do not have a VAO object, we use vertex arrays as fail safes
void glVertexArrayObject::Disable(){
	if(!_useVA){
		GLCheck(glBindVertexArray(0));
	}else{
		Disable_VA();		
	}
}

/// If we do not have a VBO object, we use vertex arrays as fail safes
void glVertexArrayObject::Enable_VA() {

	GLCheck(glEnableClientState(GL_VERTEX_ARRAY));
	GLCheck(glVertexPointer(3, GL_FLOAT, sizeof(vec3<GLfloat>), &(_dataPosition[0].x)));

	if(!_dataNormal.empty()) {
		GLCheck(glEnableClientState(GL_NORMAL_ARRAY));
		GLCheck(glNormalPointer(GL_FLOAT, sizeof(vec3<GLfloat>), &(_dataNormal[0].x)));
	}

	if(!_dataTexcoord.empty()) {
		GLCheck(glClientActiveTexture(GL_TEXTURE0));
		GLCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
		GLCheck(glTexCoordPointer(2, GL_FLOAT, sizeof(vec2<GLfloat>), &(_dataTexcoord[0].s)));
	}

	if(!_dataTangent.empty()) {
		GLCheck(glClientActiveTexture(GL_TEXTURE1));
		GLCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
		GLCheck(glTexCoordPointer(3, GL_FLOAT, sizeof(vec3<GLfloat>), &(_dataTangent[0].x)));
	}

	if(!_dataBiTangent.empty()) {
		GLCheck(glClientActiveTexture(GL_TEXTURE2));
		GLCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
		GLCheck(glTexCoordPointer(3, GL_FLOAT, sizeof(vec3<GLfloat>), &(_dataBiTangent[0].x)));
	}

	if(_animationData){
		GLCheck(glClientActiveTexture(GL_TEXTURE3));
		GLCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
		GLCheck(glTexCoordPointer(4, GL_UNSIGNED_BYTE,sizeof(vec4<GLubyte>), &(_boneIndices[0].x)));

		GLCheck(glClientActiveTexture(GL_TEXTURE4));
		GLCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
		GLCheck(glTexCoordPointer(4, GL_FLOAT, sizeof(vec4<GLfloat>), &(_boneWeights[0].x)));
	}
}

/// If we do not have a VBO object, we use vertex arrays as fail safes
void glVertexArrayObject::Disable_VA() {

	if(_animationData){
		
		GLCheck(glClientActiveTexture(GL_TEXTURE4));
		GLCheck(glDisableClientState(GL_TEXTURE_COORD_ARRAY));

		GLCheck(glClientActiveTexture(GL_TEXTURE3));
		GLCheck(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
	}

	if(!_dataBiTangent.empty()){
		GLCheck(glClientActiveTexture(GL_TEXTURE2));
		GLCheck(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
	}

	if(!_dataTangent.empty()) {
		GLCheck(glClientActiveTexture(GL_TEXTURE1));
		GLCheck(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
	}

	if(!_dataTexcoord.empty()) {
		GLCheck(glClientActiveTexture(GL_TEXTURE0));
		GLCheck(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
	}
	
	if(!_dataNormal.empty()) {
		GLCheck(glDisableClientState(GL_NORMAL_ARRAY));
	}

	GLCheck(glDisableClientState(GL_VERTEX_ARRAY));
}




void glVertexArrayObject::Enable_VBO() {

	//GLCheck(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3<GLfloat>), (const GLvoid*)_VBOoffsetPosition));
	GLCheck(glEnableClientState(GL_VERTEX_ARRAY));
	GLCheck(glVertexPointer(3, GL_FLOAT, sizeof(vec3<GLfloat>), (const GLvoid*)_VBOoffsetPosition));

	if(!_dataNormal.empty()) {
		//GLCheck(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vec3<GLfloat>), (const GLvoid*)_VBOoffsetNormal));
		GLCheck(glEnableClientState(GL_NORMAL_ARRAY));
		GLCheck(glNormalPointer(GL_FLOAT, sizeof(vec3<GLfloat>), (const GLvoid*)_VBOoffsetNormal));
	}

	if(!_dataTexcoord.empty()) {
		GLCheck(glClientActiveTexture(GL_TEXTURE0));
		GLCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
		GLCheck(glTexCoordPointer(2, GL_FLOAT, sizeof(vec2<GLfloat>), (const GLvoid*)_VBOoffsetTexcoord));
	}

	if(!_dataTangent.empty()) {
		GLCheck(glClientActiveTexture(GL_TEXTURE1));
		GLCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
		GLCheck(glTexCoordPointer(3, GL_FLOAT, sizeof(vec3<GLfloat>), (const GLvoid*)_VBOoffsetTangent));
	}

	if(!_dataBiTangent.empty()){
		GLCheck(glClientActiveTexture(GL_TEXTURE2));
		GLCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
		GLCheck(glTexCoordPointer(3, GL_FLOAT, sizeof(vec3<GLfloat>), (const GLvoid*)_VBOoffsetBiTangent));
	}
	if(_animationData){
		GLCheck(glClientActiveTexture(GL_TEXTURE3));
		GLCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
		GLCheck(glTexCoordPointer(4, GL_UNSIGNED_BYTE, sizeof(vec4<GLubyte>),  (const GLvoid*)_VBOoffsetBoneIndices));

		GLCheck(glClientActiveTexture(GL_TEXTURE4));
		GLCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
		GLCheck(glTexCoordPointer(4, GL_FLOAT, sizeof(vec4<GLfloat>), (const GLvoid*)_VBOoffsetBoneWeights));
	}
}

void glVertexArrayObject::Enable_Shader_VBO(){
	GLCheck(glEnableVertexAttribArray(Divide::GL::VERTEX_POSITION_LOCATION));
	GLCheck(glVertexAttribPointer(Divide::GL::VERTEX_POSITION_LOCATION, 3, GL_FLOAT, GL_FALSE, sizeof(vec3<GLfloat>), (const GLvoid*)_VBOoffsetPosition));
	if(!_dataNormal.empty()) {
		GLCheck(glEnableVertexAttribArray(Divide::GL::VERTEX_NORMAL_LOCATION));
		GLCheck(glVertexAttribPointer(Divide::GL::VERTEX_NORMAL_LOCATION, 
									  3,
									  GL_FLOAT,
									  GL_FALSE,
									  sizeof(vec3<GLfloat>),
									  (const GLvoid*)_VBOoffsetNormal));
	}

	if(!_dataTexcoord.empty()) {
		GLCheck(glEnableVertexAttribArray(Divide::GL::VERTEX_TEXCOORD_LOCATION));
		GLCheck(glVertexAttribPointer(Divide::GL::VERTEX_TEXCOORD_LOCATION,
									  2, 
									  GL_FLOAT, 
									  GL_FALSE, 
									  sizeof(vec2<GLfloat>), 
									  (const GLvoid*)_VBOoffsetTexcoord));
	}

	if(!_dataTangent.empty()){
		GLCheck(glEnableVertexAttribArray(Divide::GL::VERTEX_TANGENT_LOCATION));
		GLCheck(glVertexAttribPointer(Divide::GL::VERTEX_TANGENT_LOCATION,
									  3,
									  GL_FLOAT,
									  GL_FALSE,
									  sizeof(vec3<GLfloat>),
									  (const GLvoid*)_VBOoffsetTangent));
	}

	if(!_dataBiTangent.empty()){
		GLCheck(glEnableVertexAttribArray(Divide::GL::VERTEX_BITANGENT_LOCATION));
		GLCheck(glVertexAttribPointer(Divide::GL::VERTEX_BITANGENT_LOCATION,
						              3,
									  GL_FLOAT, 
									  GL_FALSE,
									  sizeof(vec3<GLfloat>),
									  (const GLvoid*)_VBOoffsetBiTangent));
	}

	if(_animationData){
		/// Bone weights
		GLCheck(glVertexAttribPointer(Divide::GL::VERTEX_BONE_WEIGHT_LOCATION,
			                          4,
									  GL_FLOAT,
									  GL_FALSE,
									  sizeof(vec4<GLfloat>),
									  (const GLvoid*)_VBOoffsetBoneWeights));
	
		/// Bone indices
		GLCheck(glVertexAttribIPointer(Divide::GL::VERTEX_BONE_INDICE_LOCATION,
			                           4,
									   GL_UNSIGNED_BYTE,
									   sizeof(vec4<GLubyte>),
									   (const GLvoid*)_VBOoffsetBoneIndices));

		GLCheck(glEnableVertexAttribArray(Divide::GL::VERTEX_BONE_WEIGHT_LOCATION));
		GLCheck(glEnableVertexAttribArray(Divide::GL::VERTEX_BONE_INDICE_LOCATION));
	}
}

//Attribute states should be encapsulated by the VAO
void glVertexArrayObject::Disable_Shader_VBO(){}
void glVertexArrayObject::Disable_VBO(){}

void glVertexArrayObject::checkStatus(){
}