attribute vec3  inVertexData;
attribute vec3  inNormalData;
attribute vec2  inTexCoordData;
attribute vec3  inTangentData;
attribute vec3  inBiTangentData;

vec4  vertexData;
vec3  normalData;
vec2  texCoordData;
vec3  tangentData;
vec3  biTangentData;

uniform bool useVBOVertexData;
uniform bool emptyNormal;
uniform bool emptyTexCoord;
uniform bool emptyTangent;
uniform bool emptyBiTangent;


void computeData(void){

	if(useVBOVertexData ){

		//vertexData = vec4(inVertexData,1.0);
		vertexData = gl_Vertex;

		if(emptyNormal){
			normalData = gl_Normal;
		}else{
			normalData  = inNormalData;
		}

		if(emptyTexCoord){
			texCoordData   = gl_MultiTexCoord0.xy;
		}else{
			texCoordData   = inTexCoordData;
		}

		if(emptyTangent){
			tangentData    = gl_MultiTexCoord1.xyz;
		}else{
			tangentData    = inTangentData;
		}

		if(emptyBiTangent){
			biTangentData  = gl_MultiTexCoord2.xyz;
		}else{
			biTangentData  = inBiTangentData;
		}

	}else{
		vertexData     = gl_Vertex;
		normalData     = gl_Normal;
		texCoordData   = gl_MultiTexCoord0.xy;
		tangentData    = gl_MultiTexCoord1.xyz;
		biTangentData  = gl_MultiTexCoord2.xyz;
	}
}


